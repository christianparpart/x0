#pragma once

#include <sys/param.h> // size_t, odd that it's not part of <stdint.h>.
#include <x0/flow/FlowType.h>
#include <stdint.h>
#include <vector>

namespace x0 {
namespace FlowVM {

enum Opcode {
    // control
    NOP = 0,        // NOP                 ; no operation
    EXIT,           // EXIT imm            ; exit program
    JMP,            // JMP imm             ; unconditional jump
    CONDBR,         // CONDBR reg, imm     ; conditional jump

    // debugging
    NTICKS,         // instruction performance counter
    NDUMPN,         // dump registers range [A .. (B - A)]

    // copy
    MOV,            // A = B

    // numerical
    IMOV,           // A = B/imm
    NCONST,         // A = numberConstants[B]
    NNEG,           // A = -A
    NADD,           // A = B + C
    NSUB,           // A = B - C
    NMUL,           // A = B * C
    NDIV,           // A = B / C
    NREM,           // A = B % C
    NSHL,           // A = B << C
    NSHR,           // A = B >> C
    NPOW,           // A = B ** C
    NAND,           // A = B & C
    NOR,            // A = B | C
    NXOR,           // A = B ^ C
    NCMPZ,          // A = B == 0
    NCMPEQ,         // A = B == C
    NCMPNE,         // A = B != C
    NCMPLE,         // A = B <= C
    NCMPGE,         // A = B >= C
    NCMPLT,         // A = B < C
    NCMPGT,         // A = B > C

    // string
    SCONST,         // A = stringConstants[B]
    SADD,           // A = B + C
    SADDMULTI,      // A = concat(B /*rbase*/, C /*count*/)
    SSUBSTR,        // A = substr(B, C /*offset*/, C+1 /*count*/)
    SCMPEQ,         // A = B == C
    SCMPNE,         // A = B != C
    SCMPLE,         // A = B <= C
    SCMPGE,         // A = B >= C
    SCMPLT,         // A = B < C
    SCMPGT,         // A = B > C
    SCMPBEG,        // A = B =^ C           /* B begins with C */
    SCMPEND,        // A = B =$ C           /* B ends with C */
    SCONTAINS,      // A = B in C           /* B is contained in C */
    SLEN,           // A = strlen(B)
    SISEMPTY,       // A = strlen(B) == 0
    SPRINT,         // puts(A)              /* prints string A to stdout */

    // IP address
    PCONST,         // A = ipconst[B]
    PCMPEQ,         // A = ip(B) == ip(C)
    PCMPNE,         // A = ip(B) != ip(C)

    // regex
    SREGMATCH,      // A = B =~ C           /* regex match against regexPool[C] */
    SREGGROUP,      // A = regex.match(B)   /* regex match result */

    // conversion
    I2S,            // A = itoa(B)
    S2I,            // A = atoi(B)
    SURLENC,        // A = urlencode(B)
    SURLDEC,        // A = urldecode(B)

    // invokation
    // CALL A = id, B = argc, C = rbase for argv
    CALL,           // [C+0] = functions[A] ([C+1 ... C+B])
    HANDLER,        // handlers[A] ([C+1 ... C+B]); if ([C+0] == true) EXIT 1
};

enum class InstructionSig {
    None = 0,   //                   ()
    R,          // reg               (A)
    RR,         // reg, reg          (AB)
    RRR,        // reg, reg, reg     (ABC)
    RI,         // reg, imm16        (AB)
    RRI,        // reg, reg, imm16   (ABC)
    IRR,        // imm16, reg, reg   (ABC)
    IIR,        // imm16, imm16, reg (ABC)
    I,          // imm16             (A)
};

typedef uint64_t Instruction;
typedef uint16_t Operand;
typedef uint16_t ImmOperand;

// --------------------------------------------------------------------------
// encoder

constexpr Instruction makeInstruction(Opcode opc) { return (Instruction) opc; }
constexpr Instruction makeInstruction(Opcode opc, Operand op1) { return (opc | (op1 << 16)); }
constexpr Instruction makeInstruction(Opcode opc, Operand op1, Operand op2) { return (opc | (op1 << 16) | (Instruction(op2) << 32)); }
constexpr Instruction makeInstruction(Opcode opc, Operand op1, Operand op2, Operand op3) { return (opc | (op1 << 16) | (Instruction(op2) << 32) | (Instruction(op3) << 48)); }

// --------------------------------------------------------------------------
// decoder

void disassemble(Instruction pc, ImmOperand ip, const char* comment = nullptr);
void disassemble(const Instruction* program, size_t n);

constexpr Opcode opcode(Instruction instr) { return static_cast<Opcode>(instr          & 0xFF); }
constexpr Operand operandA(Instruction instr) { return static_cast<Operand>((instr >> 16) & 0xFFFF); }
constexpr Operand operandB(Instruction instr) { return static_cast<Operand>((instr >> 32) & 0xFFFF); }
constexpr Operand operandC(Instruction instr) { return static_cast<Operand>((instr >> 48) & 0xFFFF); }

inline InstructionSig operandSignature(Opcode opc);
inline const char* mnemonic(Opcode opc);

// --------------------------------------------------------------------------
// tools

size_t computeRegisterCount(const Instruction* code, size_t size);
size_t registerMax(Instruction instr);
FlowType resultType(Opcode opc);

// {{{ inlines
inline InstructionSig operandSignature(Opcode opc) {
    static const InstructionSig map[] = {
        [Opcode::NOP]       = InstructionSig::None,
        // control
        [Opcode::EXIT]      = InstructionSig::I,
        [Opcode::JMP]       = InstructionSig::I,
        [Opcode::CONDBR]    = InstructionSig::RI,
        // debug
        [Opcode::NTICKS]    = InstructionSig::R,
        [Opcode::NDUMPN]    = InstructionSig::RI,
        // copy
        [Opcode::MOV]       = InstructionSig::RR,
        // numerical
        [Opcode::IMOV]      = InstructionSig::RI,
        [Opcode::NCONST]    = InstructionSig::RI,
        [Opcode::NNEG]      = InstructionSig::RR,
        [Opcode::NADD]      = InstructionSig::RRR,
        [Opcode::NSUB]      = InstructionSig::RRR,
        [Opcode::NMUL]      = InstructionSig::RRR,
        [Opcode::NDIV]      = InstructionSig::RRR,
        [Opcode::NREM]      = InstructionSig::RRR,
        [Opcode::NSHL]      = InstructionSig::RRR,
        [Opcode::NSHR]      = InstructionSig::RRR,
        [Opcode::NPOW]      = InstructionSig::RRR,
        [Opcode::NAND]      = InstructionSig::RRR,
        [Opcode::NOR]       = InstructionSig::RRR,
        [Opcode::NXOR]      = InstructionSig::RRR,
        [Opcode::NCMPZ]     = InstructionSig::RR,
        [Opcode::NCMPEQ]    = InstructionSig::RRR,
        [Opcode::NCMPNE]    = InstructionSig::RRR,
        [Opcode::NCMPLE]    = InstructionSig::RRR,
        [Opcode::NCMPGE]    = InstructionSig::RRR,
        [Opcode::NCMPLT]    = InstructionSig::RRR,
        [Opcode::NCMPGT]    = InstructionSig::RRR,
        // string
        [Opcode::SCONST]    = InstructionSig::RI,
        [Opcode::SADD]      = InstructionSig::RRR,
        [Opcode::SSUBSTR]   = InstructionSig::RRR,
        [Opcode::SCMPEQ]    = InstructionSig::RRR,
        [Opcode::SCMPNE]    = InstructionSig::RRR,
        [Opcode::SCMPLE]    = InstructionSig::RRR,
        [Opcode::SCMPGE]    = InstructionSig::RRR,
        [Opcode::SCMPLT]    = InstructionSig::RRR,
        [Opcode::SCMPGT]    = InstructionSig::RRR,
        [Opcode::SCMPBEG]   = InstructionSig::RRR,
        [Opcode::SCMPEND]   = InstructionSig::RRR,
        [Opcode::SCONTAINS] = InstructionSig::RRR,
        [Opcode::SLEN]      = InstructionSig::RR,
        [Opcode::SISEMPTY]  = InstructionSig::RR,
        [Opcode::SPRINT]    = InstructionSig::R,
        // ipaddr
        [Opcode::PCONST]    = InstructionSig::RI,
        [Opcode::PCMPEQ]    = InstructionSig::RRR,
        [Opcode::PCMPNE]    = InstructionSig::RRR,
        // regex
        [Opcode::SREGMATCH] = InstructionSig::RRR,
        [Opcode::SREGGROUP] = InstructionSig::RR,
        // conversion
        [Opcode::I2S]       = InstructionSig::RR,
        [Opcode::S2I]       = InstructionSig::RR,
        [Opcode::SURLENC]   = InstructionSig::RR,
        [Opcode::SURLDEC]   = InstructionSig::RR,
        // invokation
        [Opcode::CALL]      = InstructionSig::IIR,
        [Opcode::HANDLER]   = InstructionSig::IIR,
    };
    return map[opc];
};

inline const char* mnemonic(Opcode opc) {
    static const char* map[] = {
        [Opcode::NOP]    = "NOP",
        // control
        [Opcode::EXIT]   = "EXIT",
        [Opcode::JMP]    = "JMP",
        [Opcode::CONDBR] = "CONDBR",
        // copy
        [Opcode::MOV]    = "MOV",
        // debug
        [Opcode::NTICKS] = "NTICKS",
        [Opcode::NDUMPN] = "NDUMPN",
        // numerical
        [Opcode::IMOV]   = "IMOV",
        [Opcode::NCONST] = "NCONST",
        [Opcode::NNEG]   = "NNEG",
        [Opcode::NADD]   = "NADD",
        [Opcode::NSUB]   = "NSUB",
        [Opcode::NMUL]   = "NMUL",
        [Opcode::NDIV]   = "NDIV",
        [Opcode::NREM]   = "NREM",
        [Opcode::NSHL]   = "NSHL",
        [Opcode::NSHR]   = "NSHR",
        [Opcode::NPOW]   = "NPOW",
        [Opcode::NAND]   = "NADN",
        [Opcode::NOR]    = "NOR",
        [Opcode::NXOR]   = "NXOR",
        [Opcode::NCMPZ]  = "NCMPZ",
        [Opcode::NCMPEQ] = "NCMPEQ",
        [Opcode::NCMPNE] = "NCMPNE",
        [Opcode::NCMPLE] = "NCMPLE",
        [Opcode::NCMPGE] = "NCMPGE",
        [Opcode::NCMPLT] = "NCMPLT",
        [Opcode::NCMPGT] = "NCMPGT",
        // string
        [Opcode::SCONST]    = "SCONST",
        [Opcode::SADD]      = "SADD",
        [Opcode::SSUBSTR]   = "SSUBSTR",
        [Opcode::SCMPEQ]    = "SCMPEQ",
        [Opcode::SCMPNE]    = "SCMPNE",
        [Opcode::SCMPLE]    = "SCMPLE",
        [Opcode::SCMPGE]    = "SCMPGE",
        [Opcode::SCMPLT]    = "SCMPLT",
        [Opcode::SCMPGT]    = "SCMPGT",
        [Opcode::SCMPBEG]   = "SCMPBEG",
        [Opcode::SCMPEND]   = "SCMPEND",
        [Opcode::SCONTAINS] = "SCONTAINS",
        [Opcode::SLEN]      = "SLEN",
        [Opcode::SISEMPTY]  = "SISEMPTY",
        [Opcode::SPRINT]    = "SPRINT",
        // ipaddr
        [Opcode::PCONST]    = "PCONST",
        [Opcode::PCMPEQ]    = "PCMPEQ",
        [Opcode::PCMPNE]    = "PCMPNE",
        // regex
        [Opcode::SREGMATCH] = "SREGMATCH",
        [Opcode::SREGGROUP] = "SREGGROUP",
        // conversion
        [Opcode::I2S]       = "I2S",
        [Opcode::S2I]       = "S2I",
        [Opcode::SURLENC]   = "SURLENC",
        [Opcode::SURLDEC]   = "SURLDEC",
        // invokation
        [Opcode::CALL]      = "CALL",
        [Opcode::HANDLER]   = "HANDLER",
    };
    return map[opc];
}

inline FlowType resultType(Opcode opc) {
    static const FlowType map[] = {
        [Opcode::NOP]       = FlowType::Void,
        // control
        [Opcode::EXIT]      = FlowType::Void,
        [Opcode::JMP]       = FlowType::Void,
        [Opcode::CONDBR]    = FlowType::Void,
        // debug
        [Opcode::NTICKS]    = FlowType::Number,
        [Opcode::NDUMPN]    = FlowType::Void,
        // copy
        [Opcode::MOV]       = FlowType::Void,
        // numerical
        [Opcode::IMOV]      = FlowType::Number,
        [Opcode::NCONST]    = FlowType::Number,
        [Opcode::NNEG]      = FlowType::Number,
        [Opcode::NADD]      = FlowType::Number,
        [Opcode::NSUB]      = FlowType::Number,
        [Opcode::NMUL]      = FlowType::Number,
        [Opcode::NDIV]      = FlowType::Number,
        [Opcode::NREM]      = FlowType::Number,
        [Opcode::NSHL]      = FlowType::Number,
        [Opcode::NSHR]      = FlowType::Number,
        [Opcode::NPOW]      = FlowType::Number,
        [Opcode::NAND]      = FlowType::Number,
        [Opcode::NOR]       = FlowType::Number,
        [Opcode::NXOR]      = FlowType::Number,
        [Opcode::NCMPZ]     = FlowType::Boolean,
        [Opcode::NCMPEQ]    = FlowType::Boolean,
        [Opcode::NCMPNE]    = FlowType::Boolean,
        [Opcode::NCMPLE]    = FlowType::Boolean,
        [Opcode::NCMPGE]    = FlowType::Boolean,
        [Opcode::NCMPLT]    = FlowType::Boolean,
        [Opcode::NCMPGT]    = FlowType::Boolean,
        // string
        [Opcode::SCONST]    = FlowType::String,
        [Opcode::SADD]      = FlowType::String,
        [Opcode::SSUBSTR]   = FlowType::String,
        [Opcode::SCMPEQ]    = FlowType::Boolean,
        [Opcode::SCMPNE]    = FlowType::Boolean,
        [Opcode::SCMPLE]    = FlowType::Boolean,
        [Opcode::SCMPGE]    = FlowType::Boolean,
        [Opcode::SCMPLT]    = FlowType::Boolean,
        [Opcode::SCMPGT]    = FlowType::Boolean,
        [Opcode::SCMPBEG]   = FlowType::Boolean,
        [Opcode::SCMPEND]   = FlowType::Boolean,
        [Opcode::SCONTAINS] = FlowType::Boolean,
        [Opcode::SLEN]      = FlowType::Number,
        [Opcode::SISEMPTY]  = FlowType::Boolean,
        [Opcode::SPRINT]    = FlowType::Void,
        // ipaddr
        [Opcode::PCONST]    = FlowType::IPAddress,
        [Opcode::PCMPEQ]    = FlowType::Boolean,
        [Opcode::PCMPNE]    = FlowType::Boolean,
        // regex
        [Opcode::SREGMATCH] = FlowType::Boolean,
        [Opcode::SREGGROUP] = FlowType::String,
        // conversion
        [Opcode::I2S]       = FlowType::String,
        [Opcode::S2I]       = FlowType::Number,
        [Opcode::SURLENC]   = FlowType::String,
        [Opcode::SURLDEC]   = FlowType::String,
        // invokation
        [Opcode::CALL]      = FlowType::Void,
        [Opcode::HANDLER]   = FlowType::Void,
    };
    return map[opc];
}
// }}}

} // namespace FlowVM
} // namespace x0
