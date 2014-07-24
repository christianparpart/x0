if(ENABLE_EXAMPLES)

  # add_executable(HelloHttp hello_http.cpp)
  # target_link_libraries(HelloHttp xzero)

  add_executable(HttpService HttpService.cpp)
  target_link_libraries(HttpService xzero)

endif()
