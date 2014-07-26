if(FLOW_ENABLE_TESTS)

  add_executable(flow-test
    flow-test.cpp
    FlowLexer-test.cpp
  )
  target_link_libraries(flow-test gtest flow)

  add_custom_target(test-libflow
    DEPENDS libflow-test
    COMMAND ./libflow-test ${CMAKE_CURRENT_SOURCE_DIR}
  )

endif()
