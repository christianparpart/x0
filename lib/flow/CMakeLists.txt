if(ENABLE_XZERO_FLOW)

set(x0_flow_SRC
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

add_library(XzeroFlow SHARED ${x0_flow_SRC})
set_target_properties(XzeroFlow PROPERTIES LINK_FLAGS "${LDFLAGS}")
set_target_properties(XzeroFlow PROPERTIES VERSION ${PACKAGE_VERSION})
set_target_properties(XzeroFlow PROPERTIES COMPILE_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden -DBUILD_X0=1")
target_link_libraries(XzeroFlow LINK_PRIVATE XzeroBase dl pthread)
install(TARGETS XzeroFlow DESTINATION ${LIBDIR})

if(ENABLE_STATIC)
add_library(XzeroFlow_static STATIC ${x0_flow_SRC})
target_link_libraries(XzeroFlow_static LINK_PRIVATE XzeroBase dl pthread)
set_target_properties(XzeroFlow_static PROPERTIES LINK_FLAGS "${LDFLAGS}")
set_target_properties(XzeroFlow_static PROPERTIES VERSION ${PACKAGE_VERSION})
install(TARGETS XzeroFlow DESTINATION ${LIBDIR})
endif()

endif(ENABLE_XZERO_FLOW)
