cmake_minimum_required(VERSION 2.8)
project(xzero-flow CXX)
set(XZERO_FLOW_VERSION "0.3.0")

include(XzeroCommon)

option(ENABLE_FLOW_DIRECT_THREADED_VM "Flow VM using direct threaded mode [default: off]" OFF)

set(xzero_flow_SRC
  AST.cc
  ASTPrinter.cc
  CMakeLists.txt
  FlowCallVisitor.cc
  FlowLexer.cc
  FlowLocation.cc
  FlowParser.cc
  FlowToken.cc
  FlowType.cc
  IRGenerator.cc
  TargetCodeGenerator.cc

  ir/BasicBlock.cc
  ir/Constant.cc
  ir/ConstantArray.cc
  ir/ConstantValue.cc
  ir/Instr.cc
  ir/Instructions.cc
  ir/InstructionVisitor.cc
  ir/IRBuilder.cc
  ir/IRHandler.cc
  ir/IRProgram.cc
  ir/PassManager.cc
  ir/Value.cc

  transform/EmptyBlockElimination.cc
  transform/InstructionElimination.cc
  transform/UnusedBlockPass.cc

  vm/ConstantPool.cc
  vm/Handler.cc
  vm/Instruction.cc
  vm/Match.cc
  vm/MatchClass.cc
  vm/NativeCallback.cc
  vm/Program.cc
  vm/Runner.cc
  vm/Runtime.cc
  vm/Signature.cc
)

set(XZERO_FLOW_INCLUDE_DIRS
    ${XZERO_BASE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(XZERO_FLOW_INCLUDE_DIRS ${XZERO_FLOW_INCLUDE_DIRS} PARENT_SCOPE)

include_directories(${XZERO_FLOW_INCLUDE_DIRS})

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/sysconfig.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/sysconfig.h)

# libxzero-flow.a
add_library(xzero-flow STATIC ${xzero_flow_SRC})
#set_target_properties(xzero-flow PROPERTIES COMPILE_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden -DBUILD_XZERO_FLOW=1")
target_link_libraries(xzero-flow pthread dl xzero-base)
install(TARGETS xzero-flow DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

# headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include
        FILES_MATCHING PATTERN "*.h"
                       PATTERN "CMakeFiles" EXCLUDE)

# test-flow
file(GLOB_RECURSE xzero_flow_test_SRC "*-test.cc")
add_executable(test-flow ${xzero_flow_test_SRC})
target_link_libraries(test-flow xzero-flow xzero-base gtest gtest_main)

# pkg-config target
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/xzero-flow.pc.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/xzero-flow.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/xzero-flow.pc 
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
