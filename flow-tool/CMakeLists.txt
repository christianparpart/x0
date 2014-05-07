if(ENABLE_FLOW_TOOL)
  add_executable(flow-tool main.cpp Flower.cpp)
  target_link_libraries(flow-tool XzeroBase XzeroFlow)
endif()
