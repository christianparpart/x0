#include <x0/flow2/FlowContext.h>
#include <x0/flow2/FlowVM.h>

namespace x0 {

FlowContext::FlowContext() :
    status_(Done),
    programSize_(0),
    program_(nullptr),
    pc_(0),
    stackSize_(0),
    stack_(nullptr),
    sp_(0),
    localsSize_(0),
    locals_(nullptr)
{
}

FlowContext::~FlowContext()
{
}

void FlowContext::setProgram(const FlowProgram& program)
{
    setProgram(program.program().data(), program.programSize(), program.stackSize(), program.localsSize());
}

void FlowContext::setProgram(const uint8_t* program, size_t programSize, size_t stackSize, size_t localsSize)
{
    programSize_ = programSize;
    program_ = program;
    pc_ = 0;

    delete[] stack_;
    stackSize_ = stackSize;
    stack_ = new uint64_t[stackSize_];

    delete[] locals_;
    localsSize_ = localsSize;
    locals_ = new uint64_t[localsSize_];
}

void FlowContext::dumpInstr(bool lf)
{
    FlowInstruction opcode = static_cast<FlowInstruction>(program_[pc_]);
    const FlowInstructionInfo& info = infoOf(opcode);

    int w = 0;
    int rv = printf("%4zu: %3d %s", pc_, opcode, info.name);
    if (rv)
        w += rv;

    for (size_t i = 0; i < info.argc; ++i) {
        const uint8_t* ip = &program_[pc_ + 1 + i * 8];
        uint64_t value =
            (((uint64_t)(ip[0])) << 56)
          | (((uint64_t)(ip[1])) << 48)
          | (((uint64_t)(ip[2])) << 40)
          | (((uint64_t)(ip[3])) << 32)
          | (((uint64_t)(ip[4])) << 24)
          | (((uint64_t)(ip[5])) << 16)
          | (((uint64_t)(ip[6])) <<  8)
          | (((uint64_t)(ip[7])) <<  0);

        rv = printf(" %li", value);
        if (rv > 0)
            w += rv;
    }

    if (info.stackIn) {
        for (int i = w; i < 24; ++i)
            printf(" ");

        printf("[");
        for (int i = info.stackIn; i > 0; --i) {
            if (i != info.stackIn) printf(", ");
            printf("%li", stack_[sp_ - i]);
        }
        printf("]");
    }

    if (lf)
        printf("\n");
}

void FlowContext::run()
{
    static const void* handlers[] = {
        [FlowInstruction::Nop] = &&l_nop,

        // integer ops
        [FlowInstruction::IAdd] = &&l_iadd,
        [FlowInstruction::ISub] = &&l_isub,
        [FlowInstruction::INot] = &&i_not, // unary
        [FlowInstruction::IMul] = &&i_mul,
        [FlowInstruction::IDiv] = &&i_div,
        [FlowInstruction::IRem] = &&i_rem,
        [FlowInstruction::IShl] = &&i_shl,
        [FlowInstruction::IShr] = &&i_shr,
        [FlowInstruction::IAnd] = &&i_and,
        [FlowInstruction::IOr]  = &&i_or,
        [FlowInstruction::IXor] = &&i_xor,
        [FlowInstruction::INeg] = &&i_neg, // unary

        // control
        [FlowInstruction::Goto] = &&i_goto,
        [FlowInstruction::CondBr] = &&i_condbr,
        [FlowInstruction::Return] = &&i_return,

        // compares
        [FlowInstruction::ICmpEQ] = &&i_icmpeq,
        [FlowInstruction::ICmpNE] = &&i_icmpne,
        [FlowInstruction::ICmpLE] = &&i_icmple,
        [FlowInstruction::ICmpGE] = &&i_icmpge,
        [FlowInstruction::ICmpLT] = &&i_icmplt,
        [FlowInstruction::ICmpGT] = &&i_icmpgt,
    };

    uint64_t* data = data_;
    const uint8_t* ip = program_;

    for (;;) {
    // {{{ misc
    l_nop:
        continue;
    // }}}
    // {{{ integer ops
    l_iadd:
        data[ip[0]] = data[ip[1]] + data[ip[2]];
        ip += 3;
        continue;
    l_isub:
        data[ip[0]] = data[ip[1]] - data[ip[2]];
        ip += 3;
        continue;
    i_neg:
        data[ip[0]] = -data[ip[1]];
        ip += 2;
        continue;
    i_mul:
        data[ip[0]] = data[ip[1]] * data[ip[2]];
        ip += 3;
        continue;
    i_div:
        data[ip[0]] = data[ip[1]] / data[ip[2]];
        ip += 3;
        continue;
    i_rem:
        data[ip[0]] = data[ip[1]] % data[ip[2]];
        ip += 3;
        continue;
    i_shl:
        data[ip[0]] = data[ip[1]] << data[ip[2]];
        ip += 3;
        continue;
    i_shr:
        data[ip[0]] = data[ip[1]] >> data[ip[2]];
        ip += 3;
        continue;
    i_and:
        data[ip[0]] = data[ip[1]] & data[ip[2]];
        ip += 3;
        continue;
    i_or:
        data[ip[0]] = data[ip[1]] | data[ip[2]];
        ip += 3;
        continue;
    i_xor:
        data[ip[0]] = data[ip[1]] ^ data[ip[2]];
        ip += 3;
        continue;
    i_not:
        data[ip[0]] = ~data[ip[1]];
        ip += 2;
        continue;
    // }}}
    // {{{ control
    i_goto:
        ip += 1;
        continue;
    i_condbr:
        ip += 1;
        continue;
    i_return:
        state_ = State::Done;
        break;
    // }}}
    // {{{ compare
    i_icmpeq:
        data[ip[0]] = data[ip[1]] == data[ip[2]];
        continue;
    i_icmpne:
        data[ip[0]] = data[ip[1]] != data[ip[2]];
        continue;
    i_icmple:
        data[ip[0]] = data[ip[1]] <= data[ip[2]];
        continue;
    i_icmpge:
        data[ip[0]] = data[ip[1]] >= data[ip[2]];
        continue;
    i_icmplt:
        data[ip[0]] = data[ip[1]] < data[ip[2]];
        continue;
    i_icmpgt:
        data[ip[0]] = data[ip[1]] > data[ip[2]];
        continue;
    // }}}
    }

    while (pc_ < programSize_) {
        //dumpInstr(true);

        FlowInstruction opcode = static_cast<FlowInstruction>(read8());

        switch (opcode) {
            // {{{ nop
            case FlowInstruction::Nop:
                break;
            // }}}
            // {{{ consts
            case FlowInstruction::IConst: {
                FlowNumber value = read64();
                push((FlowNumber)value);
                break;
            }
            case FlowInstruction::IConst0:
                push((FlowNumber)0);
                break;
            case FlowInstruction::IConst1:
                push((FlowNumber)1);
                break;
            case FlowInstruction::IConst2:
                push((FlowNumber)2);
                break;
            // }}}
            // {{{ loads
            case FlowInstruction::ILoad: {
                size_t offset = read64();
                FlowNumber value = *((FlowNumber*) &locals_[offset]);
                push(value);
                break;
            }
            // }}}
            // {{{ stores
            case FlowInstruction::IStore: {
                size_t offset = read64();
                FlowNumber value = pop<FlowNumber>();
                *((FlowNumber*) &locals_[offset]) = value;
                break;
            }
            // }}}
            // {{{ integer ops
            case FlowInstruction::IAdd: {
                FlowNumber a = pop<FlowNumber>();
                FlowNumber b = pop<FlowNumber>();
                push(a + b);
                break;
            }
            case FlowInstruction::ISub: {
                FlowNumber a = pop<FlowNumber>();
                FlowNumber b = pop<FlowNumber>();
                push(a - b);
                break;
            }
            case FlowInstruction::INeg: {
                FlowNumber a = pop<FlowNumber>();
                push(-a);
                break;
            }
            case FlowInstruction::IMul: {
                FlowNumber a = pop<FlowNumber>();
                FlowNumber b = pop<FlowNumber>();
                push(a * b);
                break;
            }
            case FlowInstruction::IDiv: {
                FlowNumber a = pop<FlowNumber>();
                FlowNumber b = pop<FlowNumber>();
                push(a * b);
                break;
            }
            case FlowInstruction::IShl: {
                FlowNumber a = pop<FlowNumber>();
                FlowNumber b = pop<FlowNumber>();
                push(a << b);
                break;
            }
            case FlowInstruction::IShr: {
                FlowNumber a = pop<FlowNumber>();
                FlowNumber b = pop<FlowNumber>();
                push(a >> b);
                break;
            }
            case FlowInstruction::IRem: {
                FlowNumber a = pop<FlowNumber>();
                FlowNumber b = pop<FlowNumber>();
                push(a % b);
                break;
            }
            case FlowInstruction::IAnd: {
                FlowNumber a = pop<FlowNumber>();
                FlowNumber b = pop<FlowNumber>();
                push(a & b);
                break;
            }
            case FlowInstruction::IOr: {
                FlowNumber a = pop<FlowNumber>();
                FlowNumber b = pop<FlowNumber>();
                push(a | b);
                break;
            }
            case FlowInstruction::IXor: {
                FlowNumber a = pop<FlowNumber>();
                FlowNumber b = pop<FlowNumber>();
                push(a ^ b);
                break;
            }
            case FlowInstruction::INot: {
                FlowNumber a = pop<FlowNumber>();
                push(~a);
                break;
            }
            // }}}
            // {{{ stack operations
            case FlowInstruction::Dup: {
                push(result());
                break;
            }
            // }}}
            // {{{ control
            case FlowInstruction::Goto: {
                pc_ = read64();
                break;
            }
            case FlowInstruction::CondBr: {
                size_t target = read64();
                FlowNumber expr = pop<FlowNumber>();

                if (expr)
                    pc_ = target;

                break;
            }
            case FlowInstruction::Return: {
                status_ = Done;
                printf("return: %lli\n", result());
                return;
            }
            // }}}
            // {{{ cmp
            case FlowInstruction::ICmpEQ: {
                FlowNumber b = pop<FlowNumber>();
                FlowNumber a = pop<FlowNumber>();
                push(FlowNumber(a == b));
                break;
            }
            case FlowInstruction::ICmpNE: {
                FlowNumber b = pop<FlowNumber>();
                FlowNumber a = pop<FlowNumber>();
                push(FlowNumber(a != b));
                break;
            }
            case FlowInstruction::ICmpLE: {
                FlowNumber b = pop<FlowNumber>();
                FlowNumber a = pop<FlowNumber>();
                push(FlowNumber(a <= b));
                break;
            }
            case FlowInstruction::ICmpGE: {
                FlowNumber a = pop<FlowNumber>();
                FlowNumber b = pop<FlowNumber>();
                push(FlowNumber(a >= b));
                break;
            }
            case FlowInstruction::ICmpLT: {
                FlowNumber b = pop<FlowNumber>();
                FlowNumber a = pop<FlowNumber>();
                push(FlowNumber(a < b));
                break;
            }
            case FlowInstruction::ICmpGT: {
                FlowNumber b = pop<FlowNumber>();
                FlowNumber a = pop<FlowNumber>();
                push(FlowNumber(a > b));
                break;
            }
            // }}}
            default:
                printf("Invalid opcode 0x%02x (%d)\n", opcode, opcode);
                break;
        }
    }
}

void FlowContext::push(const char* buf, size_t length)
{
    push(FlowNumber(buf));
    push(FlowNumber(length));
}

} // namespace x0
