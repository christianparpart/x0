// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/vm/Instruction.h>
#include <flow/vm/ConstantPool.h>
#include <vector>
#include <utility>
#include <cstdlib>
#include <cstdio>
#include <climits>
#include <unordered_map>
#include <sstream>

namespace xzero::flow {

// {{{ InstructionInfo
struct InstructionInfo {
  Opcode opcode;
  const char* const mnemonic;
  OperandSig operandSig;
  int stackChange;
  LiteralType stackOutput;

  InstructionInfo() = default;
  InstructionInfo(const InstructionInfo&) = default;

  InstructionInfo(Opcode opc, const char* const m, OperandSig opsig,
                  int _stackChange, LiteralType _stackOutput)
      : opcode(opc),
        mnemonic(m),
        operandSig(opsig),
        stackChange(_stackChange),
        stackOutput(_stackOutput) {
  }
};

#define IIDEF(opcode, operandSig, stackChange, stackOutput) \
                                   { Opcode:: opcode, #opcode, OperandSig:: operandSig, stackChange, LiteralType:: stackOutput }
  // [(size_t)(Opcode:: opcode)] = { Opcode:: opcode, #opcode, OperandSig:: operandSig, stackChange, LiteralType:: stackOutput }

// OPCODE, operandSignature, stackChange
static InstructionInfo instructionInfos[] = {
  // misc
  IIDEF(NOP,       V,  0, Void),
  IIDEF(ALLOCA,    I,  0, Void),
  IIDEF(DISCARD,   I,  0, Void),

  // control
  IIDEF(EXIT,      I,  0, Void),
  IIDEF(JMP,       I,  0, Void),
  IIDEF(JN,        I, -1, Void),
  IIDEF(JZ,        I, -1, Void),

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

LiteralType resultType(Opcode opc) {
  return instructionInfos[(size_t) opc].stackOutput;
}

// ---------------------------------------------------------------------------

std::string disassemble(const Instruction* program,
                        size_t n,
                        const std::string& indent,
                        const ConstantPool* cp) {
  std::stringstream result;
  size_t i = 0;
  size_t sp = 0;
  for (const Instruction* pc = program; pc < program + n; ++pc) {
    result << indent;
    result << disassemble(*pc, i++, sp, cp);
    result << '\n';
    sp += getStackChange(*pc);
  }
  return result.str();
}

std::string disassemble(Instruction pc, size_t ip, size_t sp, const ConstantPool* cp) {
  const Opcode opc = opcode(pc);
  const Operand A = operandA(pc);
  const Operand B = operandB(pc);
  const Operand C = operandC(pc);
  const char* mnemo = mnemonic(opc);
  std::stringstream line;
  size_t n = 0;

  std::string word = fmt::format("{:<10}", mnemo);
  line << word;
  n += word.size();

  // operands
  if (cp != nullptr) {
    switch (opc) {
      case Opcode::ITLOAD: {
        line << "[";
        n++;
        const std::vector<FlowNumber>& v = cp->getIntArray(A);
        for (size_t i = 0, e = v.size(); i != e; ++i) {
          if (i) {
            line << ", ";
            n += 2;
          }
          word = std::to_string(v[i]);
          line << word;
          n += word.size();
        }
        line << "]";
        n++;
        break;
      }
      case Opcode::STLOAD: {
        line << '[';
        n++;
        const std::vector<std::string>& v = cp->getStringArray(A);
        for (size_t i = 0, e = v.size(); i != e; ++i) {
          if (i) {
            line << ", ";
            n += 2;
          }
          line << '"' << v[i] << '"';
          n += v[i].size() + 2;
        }
        line << ']';
        n += 1;
        break;
      }
      case Opcode::PTLOAD: {
        line << "[";
        n++;
        const std::vector<IPAddress>& v = cp->getIPAddressArray(A);
        for (size_t i = 0, e = v.size(); i != e; ++i) {
          if (i) {
            line << ", ";
            n += 2;
          }
          word = v[i].str();
          line << word;
          n += word.size();
        }
        line << "]";
        n++;
        break;
      }
      case Opcode::CTLOAD: {
        line << "[";
        n++;
        const std::vector<Cidr>& v = cp->getCidrArray(A);
        for (size_t i = 0, e = v.size(); i != e; ++i) {
          if (i) {
            line << ", ";
            n += 2;
          }
          word = v[i].str();
          line << word;
          n += word.size();
        }
        line << "]";
        n++;
        break;
      }
      case Opcode::LOAD:
        word = fmt::format("STACK[{}]", A);
        line << word;
        n += word.size();
        break;
      case Opcode::STORE:
        word = fmt::format("@STACK[{}]", A);
        line << word;
        n += word.size();
        break;
      case Opcode::NLOAD:
        word = std::to_string(cp->getInteger(A));
        line << word;
        n += word.size();
        break;
      case Opcode::SLOAD:
        word = fmt::format("\"{}\"", cp->getString(A));
        line << word;
        n += word.size();
        break;
      case Opcode::PLOAD:
        word = cp->getIPAddress(A).str();
        line << word;
        n += word.size();
        break;
      case Opcode::CLOAD:
        word = cp->getCidr(A).str();
        line << word;
        n += word.size();
        break;
      case Opcode::CALL:
        word = cp->getNativeFunctionSignatures()[A];
        line << word;
        n += word.size();
        break;
      case Opcode::HANDLER:
        word = cp->getNativeHandlerSignatures()[A];
        line << word;
        n += word.size();
        break;
      default:
        switch (operandSignature(opc)) {
          case OperandSig::III:
            word = fmt::format("{}, {}, {}", A, B, C);
            line << word;
            n += word.size();
            break;
          case OperandSig::II:
            word = fmt::format("{}, {}", A, B);
            line << word;
            n += word.size();
            break;
          case OperandSig::I:
            word = fmt::format("{}", A);
            line << word;
            n += word.size();
            break;
          case OperandSig::V:
            break;
        }
        break;
    }
  } else {
    switch (operandSignature(opc)) {
      case OperandSig::III:
        word = fmt::format("{}, {}, {}", A, B, C);
        line << word;
        n += word.size();
        break;
      case OperandSig::II:
        word = fmt::format("{}, {}", A, B);
        line << word;
        n += word.size();
        break;
      case OperandSig::I:
        word = fmt::format("{}", A);
        line << word;
        n += word.size();
        break;
      case OperandSig::V:
        break;
    }
  }

  while (n < 35) {
    line << ' ';
    n++;
  }

  int stackChange = getStackChange(pc);

  const uint8_t* b = (uint8_t*)&pc;
  word = fmt::format("; ip={:>3} sp={:>2} ({}{})",
                     ip, sp,
                     stackChange > 0 ? '+' :
                         stackChange < 0 ? '-' : ' ',
                     std::abs(stackChange));
  line << word;
  n += word.size();

  return line.str();
}

}  // namespace xzero::flow
