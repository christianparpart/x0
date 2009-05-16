add_subdirectory(http)
add_subdirectory(x0)

include_directories(.)
add_definitions(-Wall -std=c++0x -ggdb3 -O0)
link_libraries(dl boost_system)

add_executable(x0d x0d-main.cpp)
target_link_libraries(x0d x0)
target_link_libraries(x0d http)
