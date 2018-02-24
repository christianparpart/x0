// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/vm/Instruction.h>
#include <vector>
#include <utility>
#include <cstdlib>
#include <cstdio>
#include <climits>
#include <unordered_map>

namespace xzero {
namespace flow {
namespace vm {

InstructionSig operandSignature(Opcode opc) {
  static std::unordered_map<size_t, InstructionSig> map = {
      {Opcode::NOP, InstructionSig::None},
      // control
      {Opcode::EXIT, InstructionSig::I},
      {Opcode::JMP, InstructionSig::I},
      {Opcode::JN, InstructionSig::I},
      {Opcode::JZ, InstructionSig::I},
      // array
      {Opcode::ITCONST, InstructionSig::RI},
      {Opcode::STCONST, InstructionSig::RI},
      {Opcode::PTCONST, InstructionSig::RI},
      {Opcode::CTCONST, InstructionSig::RI},
      // numerical
      {Opcode::IPUSH, InstructionSig::I},
      {Opcode::NPUSH, InstructionSig::I},
      {Opcode::ISTORE, InstructionSig::SS},
      {Opcode::NSTORE, InstructionSig::SS},
      {Opcode::NNEG, InstructionSig::None},
      {Opcode::NNOT, InstructionSig::None},
      {Opcode::NADD, InstructionSig::None},
      {Opcode::NSUB, InstructionSig::None},
      {Opcode::NMUL, InstructionSig::None},
      {Opcode::NDIV, InstructionSig::None},
      {Opcode::NREM, InstructionSig::None},
      {Opcode::NSHL, InstructionSig::None},
      {Opcode::NSHR, InstructionSig::None},
      {Opcode::NPOW, InstructionSig::None},
      {Opcode::NAND, InstructionSig::None},
      {Opcode::NOR, InstructionSig::None},
      {Opcode::NXOR, InstructionSig::None},
      {Opcode::NCMPZ, InstructionSig::None,
      {Opcode::NCMPEQ, InstructionSig::None},
      {Opcode::NCMPNE, InstructionSig::None},
      {Opcode::NCMPLE, InstructionSig::None},
      {Opcode::NCMPGE, InstructionSig::None},
      {Opcode::NCMPLT, InstructionSig::None},
      {Opcode::NCMPGT, InstructionSig::None},
      // boolean
      {Opcode::BNOT, InstructionSig::None,
      {Opcode::BAND, InstructionSig::None},
      {Opcode::BOR, InstructionSig::None},
      {Opcode::BXOR, InstructionSig::None},
      // string
      {Opcode::SPUSH, InstructionSig::I},
      {Opcode::SADD, InstructionSig::None},
      {Opcode::SSUBSTR, InstructionSig::II},
      {Opcode::SCMPEQ, InstructionSig::None},
      {Opcode::SCMPNE, InstructionSig::None},
      {Opcode::SCMPLE, InstructionSig::None},
      {Opcode::SCMPGE, InstructionSig::None},
      {Opcode::SCMPLT, InstructionSig::None},
      {Opcode::SCMPGT, InstructionSig::None},
      {Opcode::SCMPBEG, InstructionSig::None},
      {Opcode::SCMPEND, InstructionSig::None},
      {Opcode::SCONTAINS, InstructionSig::None},
      {Opcode::SLEN, InstructionSig::None},
      {Opcode::SISEMPTY, InstructionSig::None},
      {Opcode::SMATCHEQ, InstructionSig::None},
      {Opcode::SMATCHBEG, InstructionSig::None},
      {Opcode::SMATCHEND, InstructionSig::None},
      {Opcode::SMATCHR, InstructionSig::None},
      // ipaddr
      {Opcode::PPUSH, InstructionSig::I},
      {Opcode::PCMPEQ, InstructionSig::None},
      {Opcode::PCMPNE, InstructionSig::None},
      {Opcode::PINCIDR, InstructionSig::None},
      // cidr
      {Opcode::CPUSH, InstructionSig::I},
      {Opcode::CSTORE, InstructionSig::SS},
      // regex
      {Opcode::SREGMATCH, InstructionSig::None},
      {Opcode::SREGGROUP, InstructionSig::None},
      // conversion
      {Opcode::I2S, InstructionSig::None},
      {Opcode::P2S, InstructionSig::None},
      {Opcode::C2S, InstructionSig::None},
      {Opcode::R2S, InstructionSig::None},
      {Opcode::S2I, InstructionSig::None},
      {Opcode::SURLENC, InstructionSig::None},
      {Opcode::SURLDEC, InstructionSig::None},
      // invokation
      {Opcode::CALL, InstructionSig::IIR},
      {Opcode::HANDLER, InstructionSig::IIR}, };
  return map[static_cast<size_t>(opc)];
};

const char* mnemonic(Opcode opc) {
  static std::unordered_map<size_t, const char*> map = {
      {Opcode::NOP, "NOP"},
      // control
      {Opcode::EXIT, "EXIT"},
      {Opcode::JMP, "JMP"},
      {Opcode::JN, "JN"},
      {Opcode::JZ, "JZ"},
      // copy
      {Opcode::MOV, "MOV"},
      // array
      {Opcode::ITCONST, "ITCONST"},
      {Opcode::STCONST, "STCONST"},
      {Opcode::PTCONST, "PTCONST"},
      {Opcode::CTCONST, "CTCONST"},
      // numerical
      {Opcode::IMOV, "IMOV"},
      {Opcode::NCONST, "NCONST"},
      {Opcode::NNEG, "NNEG"},
      {Opcode::NNOT, "NNOT"},
      {Opcode::NADD, "NADD"},
      {Opcode::NSUB, "NSUB"},
      {Opcode::NMUL, "NMUL"},
      {Opcode::NDIV, "NDIV"},
      {Opcode::NREM, "NREM"},
      {Opcode::NSHL, "NSHL"},
      {Opcode::NSHR, "NSHR"},
      {Opcode::NPOW, "NPOW"},
      {Opcode::NAND, "NADN"},
      {Opcode::NOR, "NOR"},
      {Opcode::NXOR, "NXOR"},
      {Opcode::NCMPZ, "NCMPZ"},
      {Opcode::NCMPEQ, "NCMPEQ"},
      {Opcode::NCMPNE, "NCMPNE"},
      {Opcode::NCMPLE, "NCMPLE"},
      {Opcode::NCMPGE, "NCMPGE"},
      {Opcode::NCMPLT, "NCMPLT"},
      {Opcode::NCMPGT, "NCMPGT"},
      // numerical (reg, imm)
      {Opcode::NIADD, "NIADD"},
      {Opcode::NISUB, "NISUB"},
      {Opcode::NIMUL, "NIMUL"},
      {Opcode::NIDIV, "NIDIV"},
      {Opcode::NIREM, "NIREM"},
      {Opcode::NISHL, "NISHL"},
      {Opcode::NISHR, "NISHR"},
      {Opcode::NIPOW, "NIPOW"},
      {Opcode::NIAND, "NIADN"},
      {Opcode::NIOR, "NIOR"},
      {Opcode::NIXOR, "NIXOR"},
      {Opcode::NICMPEQ, "NICMPEQ"},
      {Opcode::NICMPNE, "NICMPNE"},
      {Opcode::NICMPLE, "NICMPLE"},
      {Opcode::NICMPGE, "NICMPGE"},
      {Opcode::NICMPLT, "NICMPLT"},
      {Opcode::NICMPGT, "NICMPGT"},
      // boolean
      {Opcode::BNOT, "BNOT"},
      {Opcode::BAND, "BAND"},
      {Opcode::BOR, "BOR"},
      {Opcode::BXOR, "BXOR"},
      // string
      {Opcode::SCONST, "SCONST"},
      {Opcode::SADD, "SADD"},
      {Opcode::SSUBSTR, "SSUBSTR"},
      {Opcode::SCMPEQ, "SCMPEQ"},
      {Opcode::SCMPNE, "SCMPNE"},
      {Opcode::SCMPLE, "SCMPLE"},
      {Opcode::SCMPGE, "SCMPGE"},
      {Opcode::SCMPLT, "SCMPLT"},
      {Opcode::SCMPGT, "SCMPGT"},
      {Opcode::SCMPBEG, "SCMPBEG"},
      {Opcode::SCMPEND, "SCMPEND"},
      {Opcode::SCONTAINS, "SCONTAINS"},
      {Opcode::SLEN, "SLEN"},
      {Opcode::SISEMPTY, "SISEMPTY"},
      {Opcode::SMATCHEQ, "SMATCHEQ"},
      {Opcode::SMATCHBEG, "SMATCHBEG"},
      {Opcode::SMATCHEND, "SMATCHEND"},
      {Opcode::SMATCHR, "SMATCHR"},
      // ipaddr
      {Opcode::PCONST, "PCONST"},
      {Opcode::PCMPEQ, "PCMPEQ"},
      {Opcode::PCMPNE, "PCMPNE"},
      {Opcode::PINCIDR, "PINCIDR"},
      // cidr
      {Opcode::CCONST, "CCONST"},
      // regex
      {Opcode::SREGMATCH, "SREGMATCH"},
      {Opcode::SREGGROUP, "SREGGROUP"},
      // conversion
      {Opcode::I2S, "I2S"},
      {Opcode::P2S, "P2S"},
      {Opcode::C2S, "C2S"},
      {Opcode::R2S, "R2S"},
      {Opcode::S2I, "S2I"},
      {Opcode::SURLENC, "SURLENC"},
      {Opcode::SURLDEC, "SURLDEC"},
      // invokation
      {Opcode::CALL, "CALL"},
      {Opcode::HANDLER, "HANDLER"}, };
  return map[static_cast<size_t>(opc)];
}

FlowType resultType(Opcode opc) {
  static std::unordered_map<size_t, FlowType> map = {
      {Opcode::NOP, FlowType::Void},
      // control
      {Opcode::EXIT, FlowType::Void},
      {Opcode::JMP, FlowType::Void},
      {Opcode::JN, FlowType::Void},
      {Opcode::JZ, FlowType::Void},
      // copy
      {Opcode::MOV, FlowType::Void},
      // array
      {Opcode::ITCONST, FlowType::IntArray},
      {Opcode::STCONST, FlowType::StringArray},
      {Opcode::PTCONST, FlowType::IPAddrArray},
      {Opcode::CTCONST, FlowType::CidrArray},
      // numerical
      {Opcode::IMOV, FlowType::Number},
      {Opcode::NCONST, FlowType::Number},
      {Opcode::NNEG, FlowType::Number},
      {Opcode::NNOT, FlowType::Number},
      {Opcode::NADD, FlowType::Number},
      {Opcode::NSUB, FlowType::Number},
      {Opcode::NMUL, FlowType::Number},
      {Opcode::NDIV, FlowType::Number},
      {Opcode::NREM, FlowType::Number},
      {Opcode::NSHL, FlowType::Number},
      {Opcode::NSHR, FlowType::Number},
      {Opcode::NPOW, FlowType::Number},
      {Opcode::NAND, FlowType::Number},
      {Opcode::NOR, FlowType::Number},
      {Opcode::NXOR, FlowType::Number},
      {Opcode::NCMPZ, FlowType::Boolean},
      {Opcode::NCMPEQ, FlowType::Boolean},
      {Opcode::NCMPNE, FlowType::Boolean},
      {Opcode::NCMPLE, FlowType::Boolean},
      {Opcode::NCMPGE, FlowType::Boolean},
      {Opcode::NCMPLT, FlowType::Boolean},
      {Opcode::NCMPGT, FlowType::Boolean},
      // numerical (reg, imm)
      {Opcode::NIADD, FlowType::Number},
      {Opcode::NISUB, FlowType::Number},
      {Opcode::NIMUL, FlowType::Number},
      {Opcode::NIDIV, FlowType::Number},
      {Opcode::NIREM, FlowType::Number},
      {Opcode::NISHL, FlowType::Number},
      {Opcode::NISHR, FlowType::Number},
      {Opcode::NIPOW, FlowType::Number},
      {Opcode::NIAND, FlowType::Number},
      {Opcode::NIOR, FlowType::Number},
      {Opcode::NIXOR, FlowType::Number},
      {Opcode::NICMPEQ, FlowType::Boolean},
      {Opcode::NICMPNE, FlowType::Boolean},
      {Opcode::NICMPLE, FlowType::Boolean},
      {Opcode::NICMPGE, FlowType::Boolean},
      {Opcode::NICMPLT, FlowType::Boolean},
      {Opcode::NICMPGT, FlowType::Boolean},
      // boolean
      {Opcode::BNOT, FlowType::Boolean},
      {Opcode::BAND, FlowType::Boolean},
      {Opcode::BOR, FlowType::Boolean},
      {Opcode::BXOR, FlowType::Boolean},
      // string
      {Opcode::SCONST, FlowType::String},
      {Opcode::SADD, FlowType::String},
      {Opcode::SSUBSTR, FlowType::String},
      {Opcode::SCMPEQ, FlowType::Boolean},
      {Opcode::SCMPNE, FlowType::Boolean},
      {Opcode::SCMPLE, FlowType::Boolean},
      {Opcode::SCMPGE, FlowType::Boolean},
      {Opcode::SCMPLT, FlowType::Boolean},
      {Opcode::SCMPGT, FlowType::Boolean},
      {Opcode::SCMPBEG, FlowType::Boolean},
      {Opcode::SCMPEND, FlowType::Boolean},
      {Opcode::SCONTAINS, FlowType::Boolean},
      {Opcode::SLEN, FlowType::Number},
      {Opcode::SISEMPTY, FlowType::Boolean},
      {Opcode::SMATCHEQ, FlowType::Void},
      {Opcode::SMATCHBEG, FlowType::Void},
      {Opcode::SMATCHEND, FlowType::Void},
      {Opcode::SMATCHR, FlowType::Void},
      // ipaddr
      {Opcode::PCONST, FlowType::IPAddress},
      {Opcode::PCMPEQ, FlowType::Boolean},
      {Opcode::PCMPNE, FlowType::Boolean},
      {Opcode::PINCIDR, FlowType::Boolean},
      // cidr
      {Opcode::CCONST, FlowType::Cidr},
      // regex
      {Opcode::SREGMATCH, FlowType::Boolean},
      {Opcode::SREGGROUP, FlowType::String},
      // conversion
      {Opcode::I2S, FlowType::String},
      {Opcode::P2S, FlowType::String},
      {Opcode::C2S, FlowType::String},
      {Opcode::R2S, FlowType::String},
      {Opcode::S2I, FlowType::Number},
      {Opcode::SURLENC, FlowType::String},
      {Opcode::SURLDEC, FlowType::String},
      // invokation
      {Opcode::CALL, FlowType::Void},
      {Opcode::HANDLER, FlowType::Void}, };
  return map[static_cast<size_t>(opc)];
}

Buffer disassemble(Instruction pc, size_t ip, const char* comment) {
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
    case InstructionSig::None:
      break;
    case InstructionSig::R:
      rv = line.printf(" r%d", A);
      break;
    case InstructionSig::RR:
      rv = line.printf(" r%d, r%d", A, B);
      break;
    case InstructionSig::RRR:
      rv = line.printf(" r%d, r%d, r%d", A, B, C);
      break;
    case InstructionSig::RI:
      rv = line.printf(" r%d, %d", A, B);
      break;
    case InstructionSig::RII:
      rv = line.printf(" r%d, %d, %d", A, B, C);
      break;
    case InstructionSig::RIR:
      rv = line.printf(" r%d, %d, r%d", A, B, C);
      break;
    case InstructionSig::RRI:
      rv = line.printf(" r%d, r%d, %d", A, B, C);
      break;
    case InstructionSig::IRR:
      rv = line.printf(" %d, r%d, r%d", A, B, C);
      break;
    case InstructionSig::IIR:
      rv = line.printf(" %d, %d, r%d", A, B, C);
      break;
    case InstructionSig::I:
      rv = line.printf(" %d", A);
      break;
  }

  if (rv > 0) {
    n += rv;
  }

  for (; n < 30; ++n) {
    line.printf(" ");
  }

  const uint8_t* b = (uint8_t*)&pc;
  line.printf(";%4hu | %02x %02x %02x %02x %02x %02x %02x %02x", ip, b[0], b[1],
              b[2], b[3], b[4], b[5], b[6], b[7]);

  if (comment && *comment) {
    line.printf("   %s", comment);
  }

  return line;
}

Buffer disassemble(const Instruction* program, size_t n) {
  Buffer result;
  size_t i = 0;
  for (const Instruction* pc = program; pc < program + n; ++pc) {
    result.push_back(disassemble(*pc, i++));
    result.push_back("\n");
  }
  return result;
}

}  // namespace vm
}  // namespace flow
}  // namespace xzero
