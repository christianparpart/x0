// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/vm/Instruction.h>
#include <xzero-flow/vm/ConstantPool.h>
#include <vector>
#include <utility>
#include <cstdlib>
#include <cstdio>
#include <climits>
#include <unordered_map>

namespace xzero::flow::vm {

// {{{ InstructionInfo
struct InstructionInfo {
  Opcode opcode;
  const char* const mnemonic;
  OperandSig operandSig;
  int stackChange;
  FlowType stackOutput;

  InstructionInfo() = default;
  InstructionInfo(const InstructionInfo&) = default;

  InstructionInfo(Opcode opc, const char* const m, OperandSig opsig,
                  int _stackChange, FlowType _stackOutput)
      : opcode(opc),
        mnemonic(m),
        operandSig(opsig),
        stackChange(_stackChange),
        stackOutput(_stackOutput) {
  }
};

#define IIDEF(opcode, operandSig, stackChange, stackOutput) \
  [(size_t)(Opcode:: opcode)] = { Opcode:: opcode, #opcode, OperandSig:: operandSig, stackChange, FlowType:: stackOutput }
  // { Opcode:: opcode, #opcode, OperandSig:: operandSig, stackChange, FlowType:: stackOutput }

// OPCODE, operandSignature, stackChange
static InstructionInfo instructionInfos[] = {
  // misc
  IIDEF(NOP,       V,  0, Void),
  IIDEF(ALLOCA,    I,  0, Void),
  IIDEF(DISCARD,   I,  0, Void),

  // control
  IIDEF(EXIT,      I,  0, Void),
  IIDEF(JMP,       I,  0, Void),
  IIDEF(JN,        V, -1, Void),
  IIDEF(JZ,        V, -1, Void),

  // arrays
  IIDEF(ITLOAD,    I,  1, IntArray),
  IIDEF(STLOAD,    I,  1, StringArray),
  IIDEF(PTLOAD,    I,  1, IPAddrArray),
  IIDEF(CTLOAD,    I,  1, CidrArray),

  IIDEF(LOAD,      I,  1, Void),
  IIDEF(STORE,     I, -1, Void),

  // numeric
  IIDEF(ILOAD,     I,  1, Number),
  IIDEF(NLOAD,     I,  1, Number),
  IIDEF(NNEG,      V,  0, Number),
  IIDEF(NNOT,      V,  0, Number),
  IIDEF(NADD,      V, -1, Number),
  IIDEF(NSUB,      V, -1, Number),
  IIDEF(NMUL,      V, -1, Number),
  IIDEF(NDIV,      V, -1, Number),
  IIDEF(NREM,      V, -1, Number),
  IIDEF(NSHL,      V, -1, Number),
  IIDEF(NSHR,      V, -1, Number),
  IIDEF(NPOW,      V, -1, Number),
  IIDEF(NAND,      V, -1, Number),
  IIDEF(NOR,       V, -1, Number),
  IIDEF(NXOR,      V, -1, Number),
  IIDEF(NCMPZ,     V,  0, Boolean),
  IIDEF(NCMPEQ,    V, -1, Boolean),
  IIDEF(NCMPNE,    V, -1, Boolean),
  IIDEF(NCMPLE,    V, -1, Boolean),
  IIDEF(NCMPGE,    V, -1, Boolean),
  IIDEF(NCMPLT,    V, -1, Boolean),
  IIDEF(NCMPGT,    V, -1, Boolean),

  // bool
  IIDEF(BNOT,      V,  0, Boolean),
  IIDEF(BAND,      V, -1, Boolean),
  IIDEF(BOR,       V, -1, Boolean),
  IIDEF(BXOR,      V, -1, Boolean),

  // string
  IIDEF(SLOAD,     I,  1, String),
  IIDEF(SADD,      V, -1, String),
  IIDEF(SSUBSTR,   V, -2, String),
  IIDEF(SCMPEQ,    V, -1, Boolean),
  IIDEF(SCMPNE,    V, -1, Boolean),
  IIDEF(SCMPLE,    V, -1, Boolean),
  IIDEF(SCMPGE,    V, -1, Boolean),
  IIDEF(SCMPLT,    V, -1, Boolean),
  IIDEF(SCMPGT,    V, -1, Boolean),
  IIDEF(SCMPBEG,   V, -1, Boolean),
  IIDEF(SCMPEND,   V, -1, Boolean),
  IIDEF(SCONTAINS, V, -1, Boolean),
  IIDEF(SLEN,      V,  0, Number),
  IIDEF(SISEMPTY,  V,  0, Boolean),
  IIDEF(SMATCHEQ,  I, -1, Void),
  IIDEF(SMATCHBEG, I, -1, Void),
  IIDEF(SMATCHEND, I, -1, Void),
  IIDEF(SMATCHR,   I, -1, Void),

  // IP
  IIDEF(PLOAD,     I,  1, IPAddress),
  IIDEF(PCMPEQ,    V, -1, Boolean),
  IIDEF(PCMPNE,    V, -1, Boolean),
  IIDEF(PINCIDR,   V, -1, Boolean),

  // Cidr
  IIDEF(CLOAD,     I,  1, Cidr),

  // regex
  IIDEF(SREGMATCH, I,  0, Boolean),
  IIDEF(SREGGROUP, V,  0, String),

  // cast
  IIDEF(N2S,       V,  0, String),
  IIDEF(P2S,       V,  0, String),
  IIDEF(C2S,       V,  0, String),
  IIDEF(R2S,       V,  0, String),
  IIDEF(S2N,       V,  0, Number),

  // invokation
  IIDEF(CALL,      III,0, Void),
  IIDEF(HANDLER,   II, 0, Void),
};
// }}}

int getStackChange(Instruction instr) {
  Opcode opc = opcode(instr);
  switch (opc) {
    case Opcode::ALLOCA:
      return operandA(instr);
    case Opcode::DISCARD:
      return -operandA(instr);
    case Opcode::HANDLER:
      return -operandB(instr);
    case Opcode::CALL:
      // TODO: handle void/non-void functions properly
      //return 1 - operandB(instr);
      return operandC(instr) - operandB(instr);
    default:
      return instructionInfos[opc].stackChange;
  }
}

size_t computeStackSize(const Instruction* program, size_t programSize) {
  const Instruction* i = program;
  const Instruction* e = program + programSize;
  size_t stackSize = 0;
  size_t limit = 0;

  while (i != e) {
    int change = getStackChange(*i);
    stackSize += change;
    limit = std::max(limit, stackSize);
    i++;
  }

  return limit;
}

OperandSig operandSignature(Opcode opc) {
  return instructionInfos[(size_t) opc].operandSig;
};

const char* mnemonic(Opcode opc) {
  return instructionInfos[(size_t) opc].mnemonic;
}

FlowType resultType(Opcode opc) {
  return instructionInfos[(size_t) opc].stackOutput;
}

Buffer disassemble(Instruction pc, size_t ip, size_t* sp) {
  Buffer line;
  Opcode opc = opcode(pc);
  Operand A = operandA(pc);
  Operand B = operandB(pc);
  Operand C = operandC(pc);
  const char* mnemo = mnemonic(opc);
  size_t n = 0;
  int rv = 4;

  rv = line.printf("  %-10s", mnemo);
  if (rv > 0) {
    n += rv;
  }

  switch (operandSignature(opc)) {
    case OperandSig::III:
      rv = line.printf(" %d, %d, %d", A, B, C);
      break;
    case OperandSig::II:
      rv = line.printf(" %d, %d", A, B);
      break;
    case OperandSig::I:
      rv = line.printf(" %d", A);
      break;
    case OperandSig::V:
      rv = 0;
      break;
  }

  if (rv > 0) {
    n += rv;
  }

  for (; n < 30; ++n) {
    line.printf(" ");
  }

  int stackChange = getStackChange(pc);

  const uint8_t* b = (uint8_t*)&pc;
  if (sp) {
    line.printf("; ip=%-3hu sp=%-2hu (%c%d)",
                ip, *sp,
                stackChange > 0 ? '+' :
                    stackChange < 0 ? '-' : ' ',
                std::abs(stackChange));
  } else {
    line.printf("; ip=%-3hu (%c%d)",
                ip,
                stackChange > 0 ? '+' :
                    stackChange < 0 ? '-' : ' ',
                std::abs(stackChange));
  }

  *sp += stackChange;
  return line;
}

Buffer disassemble(const Instruction* program, size_t n) {
  Buffer result;
  size_t i = 0;
  size_t sp = 0;
  for (const Instruction* pc = program; pc < program + n; ++pc) {
    result.push_back(disassemble(*pc, i++, &sp));
    result.push_back("\n");
  }
  return result;
}

// ---------------------------------------------------------------------------

std::string disassemble(const Instruction* program,
                        size_t n,
                        const std::string& indent,
                        const ConstantPool& cp) {
  Buffer result;
  size_t i = 0;
  size_t sp = 0;
  for (const Instruction* pc = program; pc < program + n; ++pc) {
    result.push_back(indent);
    result.push_back(disassemble(*pc, i++, &sp, cp));
    result.push_back("\n");
  }
  return result.str();
}

std::string disassemble(Instruction pc, size_t ip, size_t* sp,
                        const ConstantPool& cp) {
  Buffer line;
  Opcode opc = opcode(pc);
  Operand A = operandA(pc);
  Operand B = operandB(pc);
  Operand C = operandC(pc);
  const char* mnemo = mnemonic(opc);
  size_t n = 0;
  int rv = 4;

  rv = line.printf("%-10s", mnemo);
  n += rv;
  rv = 0;

  // operands
  switch (opc) {
    case Opcode::ITLOAD: {
      rv = line.printf(" [");
      const auto& v = cp.getIntArray(A);
      for (size_t i = 0, e = v.size(); i != e; ++i) {
        if (i) {
          line.push_back(", ");
          rv += 2;
        }
        rv += line.printf("%lli", v[i]);
      }
      rv += line.printf("]");
      break;
    }
    case Opcode::STLOAD: {
      rv = line.printf(" [");
      const auto& v = cp.getStringArray(A);
      for (size_t i = 0, e = v.size(); i != e; ++i) {
        if (i) {
          line.push_back(", ");
          rv += 2;
        }
        rv += line.printf("\"%s\"", v[i].str().c_str());
      }
      rv += line.printf("]");
      break;
    }
    case Opcode::PTLOAD: {
      rv = line.printf(" [");
      const auto& v = cp.getIPAddressArray(A);
      for (size_t i = 0, e = v.size(); i != e; ++i) {
        if (i) {
          line.push_back(", ");
          rv += 2;
        }
        rv += line.printf("%s", v[i].c_str());
      }
      rv += line.printf("]");
      break;
    }
    case Opcode::CTLOAD: {
      rv = line.printf(" [");
      const auto& v = cp.getCidrArray(A);
      for (size_t i = 0, e = v.size(); i != e; ++i) {
        if (i) {
          line.push_back(", ");
          rv += 2;
        }
        rv += line.printf("%s", v[i].str().c_str());
      }
      rv += line.printf("]");
      break;
    }
    case Opcode::LOAD:
      rv = line.printf(" STACK[%lli]", A);
      break;
    case Opcode::STORE:
      rv = line.printf(" @STACK[%lli]", A);
      break;
    case Opcode::ILOAD:
      rv = line.printf(" %lli", A);
      break;
    case Opcode::NLOAD:
      rv = line.printf(" %lli", cp.getInteger(A));
      break;
    case Opcode::SLOAD:
      rv = line.printf(" \"%s\"", cp.getString(A).c_str());
      break;
    case Opcode::PLOAD:
      rv = line.printf(" %s", cp.getIPAddress(A).c_str());
      break;
    case Opcode::CLOAD:
      rv = line.printf(" %s", cp.getCidr(A).str().c_str());
      break;
    case Opcode::CALL:
      rv = line.printf(" %s", cp.getNativeFunctionSignatures()[A].c_str());
      break;
    case Opcode::HANDLER:
      rv = line.printf(" %s", cp.getNativeHandlerSignatures()[A].c_str());
      break;
    default:
      switch (operandSignature(opc)) {
        case OperandSig::III:
          rv = line.printf(" %d, %d, %d", A, B, C);
          break;
        case OperandSig::II:
          rv = line.printf(" %d, %d", A, B);
          break;
        case OperandSig::I:
          rv = line.printf(" %d", A);
          break;
        case OperandSig::V:
          rv = 0;
          break;
      }
      break;
  }

  if (rv > 0) {
    n += rv;
  }

  for (; n < 35; ++n) {
    line.printf(" ");
  }

  int stackChange = getStackChange(pc);

  const uint8_t* b = (uint8_t*)&pc;
  if (sp) {
    line.printf("; ip=%-3hu sp=%-2hu (%c%d)",
                ip, *sp,
                stackChange > 0 ? '+' :
                    stackChange < 0 ? '-' : ' ',
                std::abs(stackChange));
  } else {
    line.printf("; ip=%-3hu (%c%d)",
                ip,
                stackChange > 0 ? '+' :
                    stackChange < 0 ? '-' : ' ',
                std::abs(stackChange));
  }

  *sp += stackChange;
  return line.str();
}

}  // namespace xzero::flow::vm
