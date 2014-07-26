if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libbase)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libbase)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libxzero)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libxzero)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libflow)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libflow)
endif()
