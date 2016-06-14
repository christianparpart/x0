AM_CXXFLAGS = \
	-DXOPEN_SOURCE=600 -D_FILE_OFFSET_BITS=64 \
	-D_GNU_SOURCE \
	-D_LARGEFILE_SOURCE -D_LARGE_FILES \
	-Wall -Wextra -std=c++11 \
	$(PLATFORM_CXXFLAGS) \
	$(LIBPCRE_CFLAGS) \
	-I$(top_srcdir)/src -I$(top_builddir)/src

AM_LDFLAGS =  \
	$(PLATFORM_LDFLAGS) \
	$(LIBPCRE_LIBS) \
	-lz -lbzlib

# option(ENABLE_FLOW_DIRECT_THREADED_VM "Flow VM using direct threaded mode [default: off]" OFF)

# # pkg-config target
# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/xzero-flow.pc.cmake
#                ${CMAKE_CURRENT_BINARY_DIR}/xzero-flow.pc)
# install(FILES ${CMAKE_CURRENT_BINARY_DIR}/xzero-flow.pc 
#         DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)

lib_LIBRARIES = libxzeroflow.a

libxzeroflow_a_SOURCES = \
	AST.cc \
	ASTPrinter.cc \
	CMakeLists.txt \
	FlowCallVisitor.cc \
	FlowLexer.cc \
	FlowLocation.cc \
	FlowParser.cc \
	FlowToken.cc \
	FlowType.cc \
	IRGenerator.cc \
	TargetCodeGenerator.cc \
	\
	ir/BasicBlock.cc \
	ir/Constant.cc \
	ir/ConstantArray.cc \
	ir/ConstantValue.cc \
	ir/Instr.cc \
	ir/Instructions.cc \
	ir/InstructionVisitor.cc \
	ir/IRBuilder.cc \
	ir/IRHandler.cc \
	ir/IRProgram.cc \
	ir/PassManager.cc \
	ir/Value.cc \
	\
	transform/EmptyBlockElimination.cc \
	transform/InstructionElimination.cc \
	transform/UnusedBlockPass.cc \
	\
	vm/ConstantPool.cc \
	vm/Handler.cc \
	vm/Instruction.cc \
	vm/Match.cc \
	vm/MatchClass.cc \
	vm/NativeCallback.cc \
	vm/Program.cc \
	vm/Runner.cc \
	vm/Runtime.cc \
	vm/Signature.cc

