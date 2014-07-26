if(XZERO_ENABLE_TESTS)

  add_executable(xzero-test xzero-test.cpp
    http2-framing-test.cpp
    hpack-test.cpp
  )
  target_link_libraries(xzero-test gtest xzero)

  add_custom_target(test-libxzero
    DEPENDS libxzero-test
    COMMAND ./libxzero-test ${CMAKE_CURRENT_SOURCE_DIR}
  )

endif()
