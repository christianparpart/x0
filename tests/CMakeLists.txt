add_definitions(
	-pthread
	-std=c++0x
)

if(ENABLE_TESTS)

add_executable(x0-test x0-test.cpp
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

endif()
