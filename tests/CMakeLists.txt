include_directories(../src)

add_definitions(
	-Wall -Wno-deprecated
	-pthread
	-std=c++0x
)

if(BUILD_TESTS)

add_executable(x0-test x0-test.cpp
	TokenShaper-test.cpp
	Tokenizer-test.cpp
	TaggedPtr-test.cpp
	List-test.cpp
	HttpServer-test.cpp
)

target_link_libraries(x0-test XzeroBase XzeroHttp XzeroFlow gtest)

add_custom_target(test
	DEPENDS x0-test
	COMMAND ./x0-test ${CMAKE_CURRENT_SOURCE_DIR}
)

endif(BUILD_TESTS)
