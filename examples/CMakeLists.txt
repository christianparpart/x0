if(ENABLE_EXAMPLES)

  add_executable(tcp-echo-server tcp-echo-server.cpp)
  target_link_libraries(tcp-echo-server libbase)

  add_executable(tcp-echo-server-splice tcp-echo-server-splice.cpp)
  target_link_libraries(tcp-echo-server-splice libbase)

endif()
