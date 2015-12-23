set(EXAMPLES TRUE CACHE BOOL "Wheather or not to compile example program.")

if(EXAMPLES)
  include(XzeroCommon)
  include_directories(${XZERO_BASE_INCLUDE_DIRS})
  include_directories(${XZERO_FLOW_INCLUDE_DIRS})
  include_directories(${CMAKE_CURRENT_BINARY_DIR})
  include_directories(${CMAKE_CURRENT_SOURCE_DIR})

  add_executable(hello_example hello_example.cc)
  target_link_libraries(hello_example xzero-base)

  add_executable(http_lb http_lb.cc)
  target_link_libraries(http_lb xzero-base)
endif()
