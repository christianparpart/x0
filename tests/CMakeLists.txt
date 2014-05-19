if(ENABLE_TESTS)

add_executable(x0-test x0-test.cpp
  Maybe-test.cpp
  Buffer-test.cpp
  Cidr-test.cpp
  TokenShaper-test.cpp
  Tokenizer-test.cpp
  TaggedPtr-test.cpp
  List-test.cpp
  Base64-test.cpp
  HttpServer-test.cpp
  Vary-test.cpp
)

target_link_libraries(x0-test XzeroBase XzeroHttp XzeroFlow gtest)

add_custom_target(test
  DEPENDS x0-test
  COMMAND ./x0-test ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(quickie quickie.cpp)
target_link_libraries(quickie XzeroBase XzeroHttp)

endif()
