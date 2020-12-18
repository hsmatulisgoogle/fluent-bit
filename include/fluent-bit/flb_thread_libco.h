/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2019-2020 The Fluent Bit Authors
 *  Copyright (C) 2015-2018 Treasure Data Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef FLB_THREAD_LIBCO_H
#define FLB_THREAD_LIBCO_H

#include <fluent-bit/flb_info.h>
#include <fluent-bit/flb_mem.h>
#include <fluent-bit/flb_macros.h>
#include <fluent-bit/flb_log.h>

#include <monkey/mk_core.h>

#include <stdlib.h>
#include <limits.h>
#include <libco.h>

#ifdef FLB_HAVE_VALGRIND
#include <valgrind/valgrind.h>
#endif

#define FLB_THREAD_RUN_ANYWHERE      (-1)
#define FLB_THREAD_RUN_MAIN_ONLY      (-2)
struct flb_thread {

#ifdef FLB_HAVE_VALGRIND
    unsigned int valgrind_stack_id;
#endif

    /* libco 'contexts' */
    cothread_t caller;
    cothread_t callee;

    /* Worker thread id to run this thread on.
     * FLB_THREAD_RUN_ANYWHERE allows the thread to run on any worker
     * FLB_THREAD_RUN_MAIN_ONLY requires the thread to run in the main thread
     * A non-negative value corresponds to the worker id of the worker
     * that can run this thread.
     */
    int desired_worker_id;

    /* Did this coroutine return? */
    int returned;
    uint64_t return_val;

    /* Is this coroutine able to be scheduled?
     * Prevents scheduling a thread multiple times.   */
    char scheduled;

    /* Channel this thread uses for returning */
    flb_pipefd_t ret_channel;

    void *data;

    /*
     * Callback invoked before the thread is destroyed. Used to release
     * any pending info in FLB_THREAD_DATA(...).
     */
    void (*cb_destroy) (void *);
};

#ifdef FLB_CORO_STACK_SIZE
#define FLB_THREAD_STACK_SIZE      FLB_CORO_STACK_SIZE
#else
#define FLB_THREAD_STACK_SIZE      ((3 * PTHREAD_STACK_MIN) / 2)
#endif

#define FLB_THREAD_DATA(th)        (((char *) th) + sizeof(struct flb_thread))

static FLB_INLINE void flb_thread_prepare(void)
{
    pthread_key_create(&flb_thread_key, NULL);
}

static FLB_INLINE void flb_thread_yield(struct flb_thread *th, int ended)
{
    co_switch(th->caller);
}

static FLB_INLINE void flb_thread_destroy(struct flb_thread *th)
{
    if (th->cb_destroy) {
        th->cb_destroy(FLB_THREAD_DATA(th));
    }
    flb_trace("[thread] destroy thread=%p data=%p", th, FLB_THREAD_DATA(th));

#ifdef FLB_HAVE_VALGRIND
    VALGRIND_STACK_DEREGISTER(th->valgrind_stack_id);
#endif

    co_delete(th->callee);
    flb_free(th);
}


static FLB_INLINE void flb_thread_resume(struct flb_thread *th)
{
    pthread_setspecific(flb_thread_key, (void *) th);

    if(th->returned) {
        flb_error("[thread] running thread=%p that already returned", th);
        return;
    }

    th->caller = co_active();
    co_switch(th->callee);

    if(th->returned) {
        int n = flb_pipe_w(th->ret_channel, (void *) &th->return_val, sizeof(th->return_val));
        if (n == -1) {
            flb_errno();
        }
    } else {
        __atomic_clear(&th->scheduled, __ATOMIC_RELEASE);
    }

}


static FLB_INLINE void flb_thread_return(uint64_t val, struct flb_thread *th)
{
    th->returned = 1;
    th->return_val = val;

}

static FLB_INLINE struct flb_thread *flb_thread_new(size_t data_size,
                                                    void (*cb_destroy) (void *), int desired_worker_id, flb_pipefd_t ret_channel)

{
    void *p;
    struct flb_thread *th;

    /* Create a thread context and initialize */
    p = flb_malloc(sizeof(struct flb_thread) + data_size);
    if (!p) {
        flb_errno();
        return NULL;
    }

    th = (struct flb_thread *) p;
    th->cb_destroy = NULL;
    th->desired_worker_id = desired_worker_id;
    th->returned = 0;
    th->return_val = (uint64_t) 0;
    __atomic_clear(&th->scheduled, __ATOMIC_RELEASE);
    th->ret_channel = ret_channel;

    flb_trace("[thread %p] created (custom data at %p, size=%lu",
              th, FLB_THREAD_DATA(th), data_size);

    return th;
}

#endif
