include(XzeroCommon)

include_directories(${XZERO_BASE_INCLUDE_DIRS})
include_directories(${XZERO_FLOW_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/sysconfig.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/sysconfig.h)

add_executable(xurl main.cc)
target_link_libraries(xurl xzero-base)
