#ifndef STACKDRIVER_CPP_FLUSH_H
#define STACKDRIVER_CPP_FLUSH_H

#ifdef __cplusplus
#include <boost/asio.hpp>
#include <thread>
#include <vector>
#include <mutex>

class StackdriverFlushContext {
public:
  boost::asio::io_context ioc;
  std::vector<std::thread> workers;
  std::mutex mutex;

};
extern "C" {
#else
typedef struct StackdriverFlushContext StackdriverFlushContext;
#endif

#include "stackdriver.h"
StackdriverFlushContext* stackdriver_cpp_init(int num_threads);
int stackdriver_cpp_flush(struct flb_stackdriver * plg_ctx, struct flb_thread* calling_thread, const char* data, size_t data_len, const char* tag, int tag_len);
int stackdriver_cpp_stop(struct flb_stackdriver * plg_ctx, StackdriverFlushContext* flush_ctx);


#ifdef __cplusplus
}
#endif

#endif /*STACKDRIVER_CPP_FLUSH_H*/
