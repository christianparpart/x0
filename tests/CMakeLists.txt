if(ENABLE_TESTS)

add_executable(x0d-test
  x0d-test.cpp
)

target_link_libraries(x0d-test gtest pthread)

add_custom_target(test
  DEPENDS x0d-test
  COMMAND ./x0d-test ${CMAKE_CURRENT_SOURCE_DIR}
)

endif()
