set(libflow_SRC
  AST.cpp
  ASTPrinter.cpp
  FlowCallVisitor.cpp
  FlowLexer.cpp
  FlowLocation.cpp
  FlowParser.cpp
  FlowToken.cpp
  FlowType.cpp
  IRGenerator.cpp
  TargetCodeGenerator.cpp

  ir/BasicBlock.cpp
  ir/Constant.cpp
  ir/ConstantValue.cpp
  ir/ConstantArray.cpp
  ir/Instr.cpp
  ir/Instructions.cpp
  ir/InstructionVisitor.cpp
  ir/IRBuilder.cpp
  ir/IRHandler.cpp
  ir/IRProgram.cpp
  ir/PassManager.cpp
  ir/Value.cpp

  transform/EmptyBlockElimination.cpp
  transform/InstructionElimination.cpp
  transform/UnusedBlockPass.cpp

  vm/ConstantPool.cpp
  vm/Handler.cpp
  vm/Instruction.cpp
  vm/Match.cpp
  vm/MatchClass.cpp
  vm/NativeCallback.cpp
  vm/Program.cpp
  vm/Runner.cpp
  vm/Runtime.cpp
  vm/Signature.cpp
)

add_library(flow SHARED ${libflow_SRC})
set_target_properties(flow PROPERTIES VERSION ${LIBFLOW_VERSION})
set_target_properties(flow PROPERTIES COMPILE_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden -DBUILD_FLOW=1")
#set_target_properties(flow PROPERTIES LINK_FLAGS "${BASE_LDFLAGS}")
target_link_libraries(flow ${BASE_LIBRARIES})
install(TARGETS flow DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
