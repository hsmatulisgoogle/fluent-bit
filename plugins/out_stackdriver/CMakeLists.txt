set(src
  gce_metadata.c
  stackdriver_conf.c
  stackdriver.c
  stackdriver_operation.c
  stackdriver_source_location.c
  stackdriver_http_request.c
  stackdriver_timestamp.c
  stackdriver_helper.c
  stackdriver_flush.cpp
  )


FLB_PLUGIN(out_stackdriver "${src}" "${Boost_LIBRARIES}")
target_link_libraries(flb-plugin-out_stackdriver OpenSSL::Crypto)
target_link_libraries(flb-plugin-out_stackdriver OpenSSL::SSL)
