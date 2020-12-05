#define BOOST_NETWORK_ENABLE_HTTPS
#include "stackdriver_flush.h"

#include <string>
#include <iostream>
#include <thread>
#include <vector>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ssl.hpp>


using boost::asio::ip::tcp;


extern "C" StackdriverFlushContext* stackdriver_cpp_init(int num_threads) {

  StackdriverFlushContext* ctx =  new StackdriverFlushContext();
  ctx->workers.reserve(num_threads);
  for(int i = 0; i < num_threads; ++i)
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

extern "C" int stackdriver_cpp_flush(StackdriverFlushContext* ctx) {
  boost::asio::post(ctx->ioc, [](){
      std::cout<<"I am in a post call!!\n\n\n"<<std::endl;
    });
  return 0;

}

extern "C" int flush_data(){
    try {

    boost::asio::io_service io_service;
    // Get a list of endpoints corresponding to the server name.
    tcp::resolver resolver(io_service);
    std::string website = "pantheon.corp.google.com";
    std::string resource = "/";
    tcp::resolver::query query(website, "https");
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

    // Try each endpoint until we successfully establish a connection.
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::method::sslv23_client);

    boost::asio::ssl::stream<tcp::socket> socket(io_service, ssl_ctx);
    boost::asio::connect(socket.lowest_layer(), endpoint_iterator);
    socket.handshake(boost::asio::ssl::stream_base::handshake_type::client);

    // Form the request. We specify the "Connection: close" header so that the
    // server will close the socket after transmitting the response. This will
    // allow us to treat all data up until the EOF as the content.
    boost::asio::streambuf request;
    std::ostream request_stream(&request);
    request_stream << "GET "<< resource <<" HTTP/1.0\r\n";
    request_stream << "Host: " << website << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n\r\n";

    // Send the request.
    boost::asio::write(socket, request);

    // Read the response status line. The response streambuf will automatically
    // grow to accommodate the entire line. The growth may be limited by passing
    // a maximum size to the streambuf constructor.
    boost::asio::streambuf response;
    boost::asio::read_until(socket, response, "\r\n");

    // Check that response is OK.
    std::istream response_stream(&response);
    std::string http_version;
    response_stream >> http_version;
    unsigned int status_code;
    response_stream >> status_code;
    std::string status_message;
    std::getline(response_stream, status_message);
    if (!response_stream || http_version.substr(0, 5) != "HTTP/")
    {
      std::cout << "Invalid response\n";
      return 1;
    }
    if (status_code != 200)
    {
      std::cout << "Response returned with status code " << status_code << "\n";
      return 1;
    }

    // Read the response headers, which are terminated by a blank line.
    boost::asio::read_until(socket, response, "\r\n\r\n");

    // Process the response headers.
    std::string header;
    while (std::getline(response_stream, header) && header != "\r")
      std::cout << header << "\n";
    std::cout << "\n";

    // Write whatever content we already have to output.
    if (response.size() > 0)
      std::cout << &response;

    // Read until EOF, writing data to output as we go.
    boost::system::error_code error;
    while (boost::asio::read(socket, response,
          boost::asio::transfer_at_least(1), error))
      std::cout << &response;
    if (error != boost::asio::error::eof)
      throw boost::system::system_error(error);
  }
  catch (std::exception& e)
  {
    std::cout << "Exception: " << e.what() << "\n";
  }

  return 123;
}
