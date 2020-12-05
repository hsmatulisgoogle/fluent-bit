#ifndef STACKDRIVER_CPP_FLUSH_H
#define STACKDRIVER_CPP_FLUSH_H

#ifdef __cplusplus
#include <boost/asio.hpp>
#include <thread>
#include <vector>

class StackdriverFlushContext {
public:
  boost::asio::io_context ioc;
  std::vector<std::thread> workers;
};
#else
typedef struct StackdriverFlushContext StackdriverFlushContext;
#endif


#ifdef __cplusplus
extern "C" {
#endif

StackdriverFlushContext* stackdriver_cpp_init(int num_threads);
int stackdriver_cpp_flush(StackdriverFlushContext* ctx);
int stackdriver_cpp_stop(StackdriverFlushContext* ctx);


#ifdef __cplusplus
}
#endif

#endif /*STACKDRIVER_CPP_FLUSH_H*/
