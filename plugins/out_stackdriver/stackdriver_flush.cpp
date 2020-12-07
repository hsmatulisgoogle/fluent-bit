#define BOOST_NETWORK_ENABLE_HTTPS
extern "C" {
#include "stackdriver.h"
#include <fluent-bit/flb_output.h>
#include <fluent-bit/flb_output_plugin.h>
#include <fluent-bit/flb_thread.h>
}
#include "stackdriver_flush.h"

#include <string>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/bind.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>


namespace beast = boost::beast;
namespace http = beast::http;
using boost::asio::ip::tcp;

extern "C" char *get_google_token(struct flb_stackdriver *ctx);


extern "C" int stackdriver_format(struct flb_stackdriver *ctx,
                                  const char *tag, int tag_len,
                                  const char *data, size_t bytes,
                                  flb_sds_t* out_data, size_t *out_size);



extern "C" StackdriverFlushContext* stackdriver_cpp_init(int num_threads) {

  StackdriverFlushContext* ctx =  new StackdriverFlushContext();
  ctx->workers.reserve(num_threads);
  for(int i = 0; i < num_threads; i++)
  {
    ctx->workers.emplace_back(
        [ctx] {
          // Stops the asio event loop from running out of work
          boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
              work_guard = boost::asio::make_work_guard(ctx->ioc);

          ctx->ioc.run();
        });
  }

  return ctx;
}

void cpp_internal_flush(struct flb_stackdriver* plg_ctx, struct flb_thread* calling_thread, const char* data, size_t data_len, const char* tag, int tag_len){
  StackdriverFlushContext* ctx = plg_ctx->flush_ctx;
  std::cout<<"I am in a post call!!\n\n\n"<<std::endl;

  /* Get the authorization token */
  std::unique_lock<std::mutex> lock(ctx->mutex);
  char* c_token = get_google_token(plg_ctx);
  if (!c_token) {
    // flb_plg_.. ids are macros, not funcitons.
    flb_plg_error(plg_ctx->ins, "cannot retrieve oauth2 token");
    flb_output_return(FLB_RETRY, calling_thread);
    return;
  }
  std::string token = c_token;
  lock.release();

  flb_sds_t payload_buf = NULL;
  size_t payload_size = 0;
  /* Reformat msgpack to stackdriver JSON payload */
  int ret = stackdriver_format(plg_ctx, tag, tag_len,
                               data, data_len,
                               &payload_buf, &payload_size);
  if (ret != 0) {
    flb_plg_error(plg_ctx->ins, "cannot format payload JSON");
    flb_output_return(FLB_RETRY, calling_thread);
    return;
  }

  try {
  // look up endpoint
  tcp::resolver resolver(ctx->ioc);
  tcp::resolver::query query(FLB_STD_WRITE_DOMAIN, "https");
  tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

  // handshake
  boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::method::sslv23_client);
  boost::asio::ssl::stream<tcp::socket> stream(ctx->ioc, ssl_ctx);
  boost::asio::connect(stream.lowest_layer(), endpoint_iterator);
  stream.handshake(boost::asio::ssl::stream_base::handshake_type::client);

  // HTTP request
  http::request<http::string_body> req{http::verb::post, FLB_STD_WRITE_URI, 11};
  req.set(http::field::host, FLB_STD_WRITE_DOMAIN);
  req.set(http::field::user_agent, "Fluent-Bit");
  req.set(http::field::content_type, "application/json");
  req.set(http::field::authorization, token);
  req.set(http::field::content_length, payload_size);
  req.set(http::field::body, payload_buf);
  http::write(stream, req);

  // Receive the HTTP response
  beast::flat_buffer buffer;
  http::response<http::dynamic_body> res;
  http::read(stream, buffer, res);

  // Write the message to standard out
  std::cout << res << std::endl;
  } catch (std::exception& e) {
    flb_plg_error(plg_ctx->ins, "https request failed: ", e.what());
    flb_output_return(FLB_RETRY, calling_thread);
  }
  flb_sds_destroy(payload_buf);

}


extern "C" void stackdriver_cpp_flush(struct flb_stackdriver * plg_ctx, struct flb_thread* calling_thread, const char* data, size_t data_len, const char* tag, int tag_len) {
  StackdriverFlushContext* ctx = plg_ctx->flush_ctx;
  boost::asio::post(ctx->ioc, boost::bind(cpp_internal_flush, plg_ctx, calling_thread, data, data_len, tag, tag_len));
  flb_thread_yield(calling_thread, FLB_FALSE);
}

extern "C" void stackdriver_cpp_destroy(struct flb_stackdriver * plg_ctx) {
  // Just leak for now

}
