add_subdirectory(http)
add_subdirectory(x0)

include_directories(.)
add_definitions(-Wall -std=c++0x -ggdb3 -O0)
link_libraries(dl pthread boost_system http x0)

add_executable(x0d x0d-main.cpp)
