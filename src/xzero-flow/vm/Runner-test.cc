#include <xzero/testing.h>
#include <xzero-flow/vm/Instruction.h>
#include <xzero-flow/vm/Runner.h>
#include <xzero-flow/vm/Handler.h>
#include <xzero-flow/vm/Program.h>
#include <xzero-flow/vm/ConstantPool.h>

using namespace xzero;
using namespace xzero::flow;
using namespace xzero::flow::vm;

using Value = Runner::Value;
using Code = ConstantPool::Code;

std::unique_ptr<Runner> run(Code&& code) {
  ConstantPool cp;
  cp.makeInteger(3);
  cp.makeInteger(4);
  cp.setHandler("main", std::move(code));
  Program program(std::move(cp));
  std::unique_ptr<Runner> vm = program.findHandler("main")->createRunner();
  vm->run();
  return vm;
}

// {{{ numeric
TEST(flow_vm_Runner, iload) {
  auto vm = run({ makeInstruction(Opcode::ILOAD, 3),
                  makeInstruction(Opcode::EXIT, true) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(3, vm->stack(-1));
}

TEST(flow_vm_Runner, nload) {
  auto vm = run({ makeInstruction(Opcode::NLOAD, 0),
                  makeInstruction(Opcode::EXIT, true) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(3, vm->stack(-1));
}

TEST(flow_vm_Runner, nneg) {
  auto vm = run({ makeInstruction(Opcode::ILOAD, 3),
                  makeInstruction(Opcode::NNEG),
                  makeInstruction(Opcode::EXIT, true) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(-3, static_cast<FlowNumber>(vm->stack(-1)));
}

TEST(flow_vm_Runner, nnot) {
  auto vm = run({ makeInstruction(Opcode::ILOAD, 3),
                  makeInstruction(Opcode::NNOT),
                  makeInstruction(Opcode::EXIT, true) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(~3, static_cast<FlowNumber>(vm->stack(-1)));
}

TEST(flow_vm_Runner, nadd) {
  auto vm = run({ makeInstruction(Opcode::ILOAD, 3),
                  makeInstruction(Opcode::ILOAD, 4),
                  makeInstruction(Opcode::NADD),
                  makeInstruction(Opcode::EXIT, true) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(7, vm->stack(-1));
}

TEST(flow_vm_Runner, nsub) {
  auto vm = run({ makeInstruction(Opcode::ILOAD, 7),
                  makeInstruction(Opcode::ILOAD, 4),
                  makeInstruction(Opcode::NSUB),
                  makeInstruction(Opcode::EXIT, true) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(3, vm->stack(-1));
}

TEST(flow_vm_Runner, nmul) {
  auto vm = run({ makeInstruction(Opcode::ILOAD, 3),
                  makeInstruction(Opcode::ILOAD, 4),
                  makeInstruction(Opcode::NMUL),
                  makeInstruction(Opcode::EXIT, true) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(12, vm->stack(-1));
}

TEST(flow_vm_Runner, ndiv) {
  auto vm = run({ makeInstruction(Opcode::ILOAD, 12),
                  makeInstruction(Opcode::ILOAD, 4),
                  makeInstruction(Opcode::NDIV),
                  makeInstruction(Opcode::EXIT, true) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(3, vm->stack(-1));
}
// }}}
