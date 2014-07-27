if(ENABLE_TESTS)

  add_executable(libxzero-test xzero-test.cpp
    http2-framing-test.cpp
    hpack-test.cpp
  )
  target_link_libraries(libxzero-test gtest xzero)

  add_custom_target(test-libxzero
    DEPENDS libxzero-test
    COMMAND ./libxzero-test ${CMAKE_CURRENT_SOURCE_DIR}
  )

endif()
