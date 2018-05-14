#include <xzero/testing.h>
#include <flow/vm/Instruction.h>
#include <flow/vm/Runner.h>
#include <flow/vm/Handler.h>
#include <flow/vm/Program.h>
#include <flow/vm/ConstantPool.h>

using namespace xzero;
using namespace xzero::flow;

using Code = ConstantPool::Code;

std::unique_ptr<Runner> run(Code&& code) {
  ConstantPool cp;
  cp.makeInteger(3);
  cp.makeInteger(4);
  cp.setHandler("main", std::move(code));
  Program program(std::move(cp));
  std::unique_ptr<Runner> vm = std::make_unique<Runner>(program.findHandler("main"), nullptr);
  vm->run();
  return vm;
}

// {{{ numeric
TEST(flow_vm_Runner, iload) {
  auto vm = run({ makeInstruction(Opcode::ILOAD, 3) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(3, vm->stack(-1));
}

TEST(flow_vm_Runner, nload) {
  auto vm = run({ makeInstruction(Opcode::NLOAD, 0) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(3, vm->stack(-1));
}

TEST(flow_vm_Runner, nneg) {
  auto vm = run({ makeInstruction(Opcode::ILOAD, 3),
                  makeInstruction(Opcode::NNEG) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(-3, static_cast<FlowNumber>(vm->stack(-1)));
}

TEST(flow_vm_Runner, nnot) {
  auto vm = run({ makeInstruction(Opcode::ILOAD, 3),
                  makeInstruction(Opcode::NNOT) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(~3, static_cast<FlowNumber>(vm->stack(-1)));
}

TEST(flow_vm_Runner, nadd) {
  auto vm = run({ makeInstruction(Opcode::ILOAD, 3),
                  makeInstruction(Opcode::ILOAD, 4),
                  makeInstruction(Opcode::NADD) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(7, vm->stack(-1));
}

TEST(flow_vm_Runner, nsub) {
  auto vm = run({ makeInstruction(Opcode::ILOAD, 7),
                  makeInstruction(Opcode::ILOAD, 4),
                  makeInstruction(Opcode::NSUB) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(3, vm->stack(-1));
}

TEST(flow_vm_Runner, nmul) {
  auto vm = run({ makeInstruction(Opcode::ILOAD, 3),
                  makeInstruction(Opcode::ILOAD, 4),
                  makeInstruction(Opcode::NMUL) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(12, vm->stack(-1));
}

TEST(flow_vm_Runner, ndiv) {
  auto vm = run({ makeInstruction(Opcode::ILOAD, 12),
                  makeInstruction(Opcode::ILOAD, 4),
                  makeInstruction(Opcode::NDIV) });

  ASSERT_EQ(1, vm->getStackPointer());
  EXPECT_EQ(3, vm->stack(-1));
}
// }}}
