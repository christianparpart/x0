// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/LiteralType.h>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace xzero::flow {

class ConstantPool;

enum class OperandSig {
  V,        // no operands
  I,        // imm16
  II,       // imm16, imm16
  III,      // imm16, imm16, imm16
};

enum Opcode : uint16_t {
  // misc
  NOP = 0,  // NOP                 ; no operation
  ALLOCA,   // ALLOCA imm         ; pushes A default-initialized items onto the stack
  DISCARD,  // DISCARD imm        ; pops A items from the stack

  // control
  EXIT,     // EXIT imm           ; exit program
  JMP,      // JMP imm            ; unconditional jump to A
  JN,       // JN imm             ; conditional jump to A if (pop() != 0)
  JZ,       // JZ imm             ; conditional jump to A if (pop() == 0)

  // const arrays
  ITLOAD,   // stack[sp++] = intArray[imm]
  STLOAD,   // stack[sp++] = stringArray[imm]
  PTLOAD,   // stack[sp++] = ipaddrArray[imm]
  CTLOAD,   // stack[sp++] = cidrArray[imm]

  LOAD,     // LOAD imm, imm      ; stack[++op1] = stack[op2]
  STORE,    // STORE imm, imm     ; stack[op1] = stack[op2]

  // numerical
  ILOAD,    // ILOAD imm
  NLOAD,    // NLOAD numberConstants[imm]
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
  SADD,       // b = pop(); a = pop(); push(a + b);
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
  PCMPEQ,    // A = ip(B) == ip(C)
  PCMPNE,    // A = ip(B) != ip(C)
  PINCIDR,   // A = cidr(C).contains(ip(B))

  // CIDR
  CLOAD,    // CLOAD  cidrConstants[imm]

  // regex
  SREGMATCH,  // A = B =~ C           /* regex match against regexPool[C] */
  SREGGROUP,  // A = regex.match(B)   /* regex match result */

  // conversion
  N2S,      // push(itoa(pop()))
  P2S,      // push(ip(pop()).toString())
  C2S,      // push(cidr(pop()).toString()
  R2S,      // push(regex(pop()).toString()
  S2N,      // push(atoi(pop()))

  // invokation
  // CALL A = id, B = argc
  CALL,     // calls A with B arguments, always pushes result to stack
  HANDLER,  // calls A with B arguments (never leaves result on stack)
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

/** Determines the operand signature of the given instruction. */
OperandSig operandSignature(Opcode opc);

/** Returns the mnemonic string representing the opcode. */
const char* mnemonic(Opcode opc);

/**
 * Determines the data type of the result being pushed onto the stack, if any.
 */
LiteralType resultType(Opcode opc);

/**
 * Computes the stack height after the execution of the given instruction.
 */
int getStackChange(Instruction instr);

/**
 * Computes the highest stack size needed to run the given program.
 */
size_t computeStackSize(const Instruction* program, size_t programSize);

/**
 * Disassembles the @p program with @p n instructions.
 *
 * @param program pointer to the first instruction to disassemble
 * @param n       number of instructions to disassemble
 * @param indent  prefix to inject in front of every new instruction line
 * @param cp      pointer to ConstantPool for pretty-printing or @c nullptr 
 *
 * @returns       disassembled program in text form.
 */
std::string disassemble(const Instruction* program, size_t n,
                        const std::string& indent,
                        const ConstantPool* cp);

/**
 * Disassembles a single instruction.
 *
 * @param pc      Instruction to disassemble.
 * @param ip      The instruction pointer at which position the instruction is
 *                located within the program.
 * @param sp      current stack size (depth) before executing given instruction.
 *                This value will be modified as if the instruction would have
 *                been executed.
 * @param cp      pointer to ConstantPool for pretty-printing or @c nullptr 
 */
std::string disassemble(Instruction pc, size_t ip, size_t sp, const ConstantPool* cp);

} // namespace xzero::flow
