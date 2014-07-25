if(FLOW_ENABLE_TESTS)

  add_executable(flow-test
    flow-test.cpp
    FlowLexer-test.cpp
  )
  target_link_libraries(flow-test gtest flow)

endif()
