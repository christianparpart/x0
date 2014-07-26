if(ENABLE_EXAMPLES)

  add_executable(hello_http hello_http.cpp)
  target_link_libraries(hello_http xzero)

  add_executable(hello_service hello_service.cpp)
  target_link_libraries(hello_service xzero)

endif()
