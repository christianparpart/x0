if(ENABLE_TESTS)

add_executable(libbase-test libbase-test.cpp
  Maybe-test.cpp
  Try-test.cpp
  Buffer-test.cpp
  Cidr-test.cpp
  TokenShaper-test.cpp
  Tokenizer-test.cpp
  TaggedPtr-test.cpp
  List-test.cpp
  Base64-test.cpp
)

target_link_libraries(libbase-test base gtest)

add_custom_target(test
  DEPENDS libbase-test
  COMMAND ./libbase-test ${CMAKE_CURRENT_SOURCE_DIR}
)

endif()
