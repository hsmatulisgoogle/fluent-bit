/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit Demo
 *  ===============
 *  Copyright (C) 2015 Treasure Data Inc.
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
#include <fluent-bit/flb_info.h>
#include <fluent-bit/flb_mem.h>
#include <fluent-bit/flb_sds.h>
#include <fluent-bit/flb_error.h>
#include <fluent-bit/flb_utils.h>
#include <fluent-bit/flb_sds.h>
#include <fluent-bit/flb_time.h>
#include <fluent-bit/flb_pack.h>
#include <fluent-bit/flb_unescape.h>
#include <fluent-bit.h>

#include <msgpack.h>

#include <stdio.h>
#include <string.h>

// max json size
#define BUF_SIZE 1024

int main()
{

    // Read JSON in from stdin
    char buffer[BUF_SIZE];
    size_t loc = 0;
    buffer[0] = '\0';
    while(fgets(buffer + loc, BUF_SIZE - 1 - loc, stdin)) {
        loc = strlen(buffer);
    }
    
    // json to be used
    char* json = buffer;
    
    char* mp_buf;
    size_t mp_size;
    int mp_type;
    if(flb_pack_json(json, strlen(json), &mp_buf, &mp_size, &mp_type)){
        printf("error!!!!! can't pack json\n");
        return 1;
    }

    volatile flb_sds_t out_json = flb_msgpack_raw_to_json_sds(mp_buf, mp_size);
    printf("%s\n", out_json);
    flb_sds_destroy(out_json);

    // Convert json to msgpack format
    for(int i=0; i < 1000000001; i++){
        out_json = flb_msgpack_raw_to_json_sds(mp_buf, mp_size);
        if (i % 10000000 == 0){
            printf("%d\n", i);
            printf("%s\n", out_json);
        }
        flb_sds_destroy(out_json);
    }

    return 0;
}
