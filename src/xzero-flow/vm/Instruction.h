// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/FlowType.h>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace xzero::flow::vm {

enum class StackSig {
//SIG          OUTPUT   INPUT
  V_V,      // void     void
  V_N,      // void     num, num
  V_S,      // void     str
  V_P,      // void     ip
  V_C,      // void     cidr
  V_X,      // void     variable number of values
  X_X,      // value?   variable number of values

  N_V,      // num      void
  N_N,      // num      num
  N_NN,     // num      num, num
  N_S,      // num      str

  B_N,      // bool     num
  B_NN,     // bool     num, num
  B_B,      // bool     bool
  B_BB,     // bool     bool, bool
  B_S,      // bool     str
  B_PP,     // bool     ip, ip
  B_PC,     // bool     ip, cidr
  B_SR,     // bool     str, regex
  B_SS,     // bool     str, str

  S_V,      // str      void
  S_N,      // str      num
  S_P,      // str      ip
  S_C,      // str      cidr
  S_R,      // str      regex
  S_S,      // str      str
  S_SS,     // str      str, str
  S_SNN,    // str      str, num, num

  P_V,      // ip       void
  C_V,      // cidr     void
};

enum class OperandSig {
  V,        // no operands

  I,        // imm16 value        imm

  N,        // const number       numberConstants[imm]
  S,        // const string       stringConstants[imm]
  P,        // const IP           ipaddrConstants[imm]
  C,        // const Cidr         cidrConstants[imm]
  R,        // const RegExp       regexpConstants[imm]

  n,        // stack num          STACK[imm] AS NUMBER
  s,        // stack string       STACK[imm] AS STRING
  p,        // stack IP           STACK[imm] AS IP
  c,        // stack Cidr         STACK[imm] AS CIDR
};

enum Opcode : uint16_t {
  // misc
  NOP = 1,  // NOP                 ; no operation

  // stack manip
  DISCARD,  // DISCARD imm        ; pops A items from the stack

  // control
  EXIT,     // EXIT imm           ; exit program
  JMP,      // JMP imm            ; unconditional jump to A
  JN,       // JN imm             ; conditional jump to A if (pop() != 0)
  JZ,       // JZ imm             ; conditional jump to A if (pop() == 0)

  // const arrays
  ITSTORE,  // ITSTORE stack[imm], intArray[imm]
  STSTORE,  // STSTORE stack[imm], stringArray[imm]
  PTSTORE,  // PTSTORE stack[imm], ipaddrArray[imm]
  CTSTORE,  // CTSTORE stack[imm], cidrArray[imm]

  LOAD,     // LOAD imm, imm      ; stack[++op1] = stack[op2]
  STORE,    // STORE imm, imm     ; stack[op1] = stack[op2]

  // numerical
  ILOAD,    // ILOAD imm
  NLOAD,    // NLOAD numberConstants[imm]
  ISTORE,   // ISTORE stack[imm], imm
  NSTORE,   // NSTORE stack[imm] # from numberConstants[stack[SP--]]
  NNEG,     //                    ; stack[SP] = -stack[SP]
  NNOT,     //                    ; stack[SP] = ~stack[SP]
  NADD,     //                    ; npush(npop() + npop())
  NSUB,     //                    ; npush(npop() - npop())
  NMUL,     //                    ; npush(npop() * npop())
  NDIV,     //                    ; npush(npop() / npop())
  NREM,     //                    ; npush(npop() % npop())
  NSHL,     // t = stack[SP-2] << stack[SP-1]; pop(2); npush(t);
  NSHR,     // A = B >> C
  NPOW,     // A = B ** C
  NAND,     // A = B & C
  NOR,      // A = B | C
  NXOR,     // A = B ^ C
  NCMPZ,    // A = B == 0
  NCMPEQ,   // A = B == C
  NCMPNE,   // A = B != C
  NCMPLE,   // A = B <= C
  NCMPGE,   // A = B >= C
  NCMPLT,   // A = B < C
  NCMPGT,   // A = B > C

  // boolean
  BNOT,  // A = !A
  BAND,  // A = B and C
  BOR,   // A = B or C
  BXOR,  // A = B xor C

  // string
  SLOAD,      // SLOAD stringConstants[imm]
  SSTORE,     // SSTORE stack[imm] # from stringConstants[stack[SP--]]
  SADD,       // b = pop(); a = pop(); push(a + b);
  SADDMULTI,  // A = concat(B /*rbase*/, C /*count*/)
  SSUBSTR,    // A = substr(B, C /*offset*/, C+1 /*count*/)
  SCMPEQ,     // A = B == C
  SCMPNE,     // A = B != C
  SCMPLE,     // A = B <= C
  SCMPGE,     // A = B >= C
  SCMPLT,     // A = B < C
  SCMPGT,     // A = B > C
  SCMPBEG,    // A = B =^ C           /* B begins with C */
  SCMPEND,    // A = B =$ C           /* B ends with C */
  SCONTAINS,  // A = B in C           /* B is contained in C */
  SLEN,       // A = strlen(B)
  SISEMPTY,   // A = strlen(B) == 0
  SMATCHEQ,   // $pc = MatchSame[A].evaluate(B);
  SMATCHBEG,  // $pc = MatchBegin[A].evaluate(B);
  SMATCHEND,  // $pc = MatchEnd[A].evaluate(B);
  SMATCHR,    // $pc = MatchRegEx[A].evaluate(B);

  // IP address
  PLOAD,     // PLOAD ipaddrConstants[imm]
  PSTORE,    // PSTORE stack[imm] # from ipaddrConstants[stack[SP--]]
  PCMPEQ,    // A = ip(B) == ip(C)
  PCMPNE,    // A = ip(B) != ip(C)
  PINCIDR,   // A = cidr(C).contains(ip(B))

  // CIDR
  CLOAD,    // CLOAD  cidrConstants[imm]
  CSTORE,   // CSTORE stack[imm] # from cidrConstants[stack[SP--]]

  // regex
  SREGMATCH,  // A = B =~ C           /* regex match against regexPool[C] */
  SREGGROUP,  // A = regex.match(B)   /* regex match result */

  // conversion
  N2S,      // push(itoa(pop()))
  P2S,      // push(ip(pop()).toString())
  C2S,      // push(cidr(pop()).toString()
  R2S,      // push(regex(pop()).toString()
  S2I,      // push(atoi(pop()))
  SURLENC,  // push(urlencode(pop())
  SURLDEC,  // push(urldecode(pop())

  // invokation
  // CALL A = id, B = argc, C = rbase for argv
  CALL,     // [C+0] = functions[A] ([C+1 ... C+B])
  HANDLER,  // handlers[A] ([C+1 ... C+B]); if ([C+0] == true) EXIT 1

  _COUNT,
};

enum class InstructionSig {
  None = 0,   //                      ()
  I,          // imm16                (A)
  II,         // imm16, imm16         (AB)
  III,        // imm16, imm16, imm16  (ABC)
  S,          // stack                (A)
  SS,         // stack, stack         (AB)
  SSS,        // stack, stack, stack  (ABC)
  SI,         // stack, imm16         (AB)
  SSI,        // stack, stack, imm16  (ABC)
  SII,        // stack, imm16, imm16  (ABC)
};

typedef uint64_t Instruction;
typedef uint16_t Operand;

// --------------------------------------------------------------------------
// encoder

/** Creates an instruction with no operands. */
constexpr Instruction makeInstruction(Opcode opc) {
  return (Instruction) opc;
}

/** Creates an instruction with one operand. */
constexpr Instruction makeInstruction(Opcode opc, Operand op1) {
  return (opc | (op1 << 16));
}

/** Creates an instruction with two operands. */
constexpr Instruction makeInstruction(Opcode opc, Operand op1, Operand op2) {
  return (opc
       | (op1 << 16)
       | (Instruction(op2) << 32));
}

/** Creates an instruction with three operands. */
constexpr Instruction makeInstruction(Opcode opc, Operand op1, Operand op2,
                                      Operand op3) {
  return (opc
       | (op1 << 16)
       | (Instruction(op2) << 32)
       | (Instruction(op3) << 48));
}

// --------------------------------------------------------------------------
// decoder

/**
 * Disassembles the @p program with @p n instructions.
 *
 * @param program pointer to the first instruction to disassemble
 * @param n       number of instructions to disassemble
 *
 * @returns       disassembled program in text form.
 */
Buffer disassemble(const Instruction* program, size_t n);

/**
 * Disassembles a single instruction.
 *
 * @param pc      Instruction to disassemble.
 * @param ip      The instruction pointer at which position the instruction is
 *                located within the program.
 * @param comment An optional comment to append to the end of the disassembly.
 */
Buffer disassemble(Instruction pc, size_t ip, const char* comment = nullptr);

/** decodes the opcode from the instruction. */
constexpr Opcode opcode(Instruction instr) {
  return static_cast<Opcode>(instr & 0xFF);
}

/** Decodes the first operand from the instruction. */
constexpr Operand operandA(Instruction instr) {
  return static_cast<Operand>((instr >> 16) & 0xFFFF);
}

/** Decodes the second operand from the instruction. */
constexpr Operand operandB(Instruction instr) {
  return static_cast<Operand>((instr >> 32) & 0xFFFF);
}

/** Decodes the third operand from the instruction. */
constexpr Operand operandC(Instruction instr) {
  return static_cast<Operand>((instr >> 48) & 0xFFFF);
}

InstructionSig operandSignature(Opcode opc);
const char* mnemonic(Opcode opc);
FlowType resultType(Opcode opc);

} // namespace xzero::flow::vm
