cmake_minimum_required(VERSION 2.8)
project(x0d CXX C)

# add_definitions(-DPROJECT_ROOT_SRC_DIR=${CMAKE_CURRENT_SOURCE_DIR})
# add_definitions(-DPROJECT_ROOT_BIN_DIR=${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/gtest-1.7.0/include)
#add_subdirectory(3rdparty/gtest-1.7.0)
add_subdirectory(3rdparty/libxzero-base/src/xzero)
add_subdirectory(3rdparty/libxzero-flow/src/xzero-flow)
add_subdirectory(src)

add_custom_target(ctags
    COMMAND ctags -R --c++-kinds=+p --fields=+iaS --extra=+q ${CMAKE_SOURCE_DIR}/lib ${CMAKE_SOURCE_DIR}/include
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_custom_target(cscope
    COMMAND echo -n > ${CMAKE_SOURCE_DIR}/cscope.files
            && find ${CMAKE_SOURCE_DIR}/lib -name '*.h' -o -name '*.cc' -o -name '*.cpp' >> ${CMAKE_SOURCE_DIR}/cscope.files
            && find ${CMAKE_SOURCE_DIR}/include -name '*.h' -o -name '*.cc' -o -name '*.hpp' >> ${CMAKE_SOURCE_DIR}/cscope.files
            && cscope -q -R -b -i cscope.files
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(release
    COMMAND ${CMAKE_SOURCE_DIR}/contrib/ppa-release.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

