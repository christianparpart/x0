if(XZERO_ENABLE_TESTS)

  add_executable(xzero-test xzero-test.cpp
    http2-framing-test.cpp
    hpack-test.cpp
  )
  target_link_libraries(xzero-test gtest xzero)

endif()
