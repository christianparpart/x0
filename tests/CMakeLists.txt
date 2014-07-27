if(ENABLE_TESTS)

  add_executable(libflow-test
    flow-test.cpp
    FlowLexer-test.cpp
  )
  target_link_libraries(libflow-test gtest flow)

  add_custom_target(test-libflow
    DEPENDS libflow-test
    COMMAND ./libflow-test ${CMAKE_CURRENT_SOURCE_DIR}
  )

endif()
