// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/NativeCallback.h>
#include <flow/Params.h>
#include <flow/util/strings.h>
#include <flow/vm/Handler.h>
#include <flow/vm/Instruction.h>
#include <flow/vm/Match.h>
#include <flow/vm/Program.h>
#include <flow/vm/Runner.h>

#include <xzero/sysconfig.h>

#include <vector>
#include <utility>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <inttypes.h>

// XXX Visual Studio doesn't support computed goto statements
#if defined(_MSC_VER)
#define FLOW_VM_LOOP_SWITCH 1
#endif

#define FLOW_DEBUG(msg, ...) do {} while (1)

namespace xzero::flow {

// {{{ VM helper preprocessor definitions
#define OP opcode((Instruction) *pc)
#define A  operandA((Instruction) *pc)
#define B  operandB((Instruction) *pc)
#define C  operandC((Instruction) *pc)

#define SP(i)           stack_[(i)]
#define popStringPtr()  ((FlowString*) stack_.pop())
#define incr_pc()       do { ++pc; } while (0)
#define jump_to(offset) do { set_pc(offset); jump; } while (0)
#define tracelog()      do { traceLogger_((Instruction) *pc, get_pc(), stack_.size()); } while (0)

#if defined(FLOW_VM_LOOP_SWITCH)
  #define LOOP_BEGIN()    for (;;) { switch (OP) {
  #define LOOP_END()      default: FLOW_ASSERT(false, "Unknown Opcode hit!"); } }
  #define instr(NAME)     case NAME: tracelog();
  #define get_pc()        (pc - code.data())
  #define set_pc(offset)  do { pc = code.data() + (offset); } while (0)
  #define jump            if (true) { break; }
  #define next            if (true) { ++pc; break; }
#elif defined(ENABLE_FLOW_DIRECT_THREADED_VM)
  #define LOOP_BEGIN()    jump;
  #define LOOP_END()
  #define instr(name)     l_##name : ++pc; tracelog();
  #define get_pc()        ((pc - code.data()) / 2)
  #define set_pc(offset)  do { pc = code.data() + (offset) * 2; } while (0)
  #define jump            goto* (void*) *pc
  #define next            goto* (void*) *++pc
#else
  #define LOOP_BEGIN()    jump;
  #define LOOP_END()
  #define instr(name)     l_##name : tracelog();
  #define get_pc()        (pc - code.data())
  #define set_pc(offset)  do { pc = code.data() + (offset); } while (0)
  #define jump            goto* ops[OP]
  #define next            goto* ops[opcode(*++pc)]
#endif
// }}}

static FlowString* t = nullptr;

Runner::Runner(const Handler* handler, void* userdata, TraceLogger traceLogger)
    : handler_(handler),
      traceLogger_{traceLogger ? traceLogger : [](Instruction, size_t, size_t) {}},
      program_(handler->program()),
      userdata_(userdata),
      regexpContext_(),
      state_(Inactive),
      ip_(0),
      stack_(handler_->stackSize()),
      stringGarbage_() {
  // initialize emptyString()
  t = newString("");
}

Runner::~Runner() {}

FlowString* Runner::newString(std::string value) {
  stringGarbage_.emplace_back(std::move(value));
  return &stringGarbage_.back();
}

FlowString* Runner::catString(const FlowString& a, const FlowString& b) {
  stringGarbage_.emplace_back(a + b);
  return &stringGarbage_.back();
}

bool Runner::run() {
  assert(state_ == Inactive);
  return loop();
}

void Runner::suspend() {
  assert(state_ == Running);
  state_ = Suspended;
}

bool Runner::resume() {
  assert(state_ == Suspended);
  return loop();
}

void Runner::rewind() {
  ip_ = 0;
}

bool Runner::loop() {
  state_ = Running;

#if !defined(FLOW_VM_LOOP_SWITCH)
// {{{ jump table
#define label(opcode) &&l_##opcode
  static const void* const ops[] = {
      // misc
      label(NOP),       label(ALLOCA),    label(DISCARD),

      // control
      label(EXIT),
      label(JMP),       label(JN),        label(JZ),

      // array
      label(ITLOAD),    label(STLOAD),    label(PTLOAD),    label(CTLOAD),

      // load'n'store
      label(LOAD),      label(STORE),

      // numerical
      label(ILOAD),     label(NLOAD),     label(NNEG),      label(NNOT),
      label(NADD),      label(NSUB),      label(NMUL),      label(NDIV),
      label(NREM),      label(NSHL),      label(NSHR),      label(NPOW),
      label(NAND),      label(NOR),       label(NXOR),      label(NCMPZ),
      label(NCMPEQ),    label(NCMPNE),    label(NCMPLE),    label(NCMPGE),
      label(NCMPLT),    label(NCMPGT),

      // boolean op
      label(BNOT),      label(BAND),      label(BOR),       label(BXOR),

      // string op
      label(SLOAD),     label(SADD),      label(SSUBSTR),
      label(SCMPEQ),    label(SCMPNE),    label(SCMPLE),    label(SCMPGE),
      label(SCMPLT),    label(SCMPGT),    label(SCMPBEG),   label(SCMPEND),
      label(SCONTAINS), label(SLEN),      label(SISEMPTY),  label(SMATCHEQ),
      label(SMATCHBEG), label(SMATCHEND), label(SMATCHR),

      // ipaddr
      label(PLOAD),     label(PCMPEQ),    label(PCMPNE),    label(PINCIDR),

      // cidr
      label(CLOAD),

      // regex
      label(SREGMATCH), label(SREGGROUP),

      // conversion
      label(N2S),       label(P2S),       label(C2S),       label(R2S),
      label(S2N),

      // invokation
      label(CALL),      label(HANDLER), };
// }}}
// {{{ direct threaded code initialization
#if !defined(ENABLE_FLOW_DIRECT_THREADED_VM)
  const std::vector<Instruction>& code = handler_->code();
#else
  std::vector<uint64_t>& code = handler_->directThreadedCode();
  if (code.empty()) {
    const std::vector<Instruction>& source = handler_->code();
    code.resize(source.size() * 2);

    uint64_t* pc = code.data();
    for (size_t i = 0, e = source.size(); i != e; ++i) {
      Instruction instr = source[i];

      *pc++ = (uint64_t)ops[opcode(instr)];
      *pc++ = instr;
    }
  }
#endif
  // }}}
#endif
  const auto* pc = code.data();
  set_pc(ip_);

  LOOP_BEGIN()

  // {{{ misc
  instr(NOP) {
    next;
  }

  instr(ALLOCA) {
    for (int i = 0; i < A; ++i)
      stack_.push(0);
    next;
  }

  instr(DISCARD) {
    stack_.discard(A);
    next;
  }
  // }}}
  // {{{ control
  instr(EXIT) {
    state_ = Inactive;
    ip_ = get_pc();
    return A != 0;
  }

  instr(JMP) {
    jump_to(A);
  }

  instr(JN) {
    if (pop() != 0) {
      jump_to(A);
    } else {
      next;
    }
  }

  instr(JZ) {
    if (pop() == 0) {
      jump_to(A);
    } else {
      next;
    }
  }
  // }}}
  // {{{ array
  instr(ITLOAD) {
    push(reinterpret_cast<Value>(&program()->constants().getIntArray(A)));
    next;
  }
  instr(STLOAD) {
    push(reinterpret_cast<Value>(&program()->constants().getStringArray(A)));
    next;
  }
  instr(PTLOAD) {
    push(reinterpret_cast<Value>(&program()->constants().getIPAddressArray(A)));
    next;
  }
  instr(CTLOAD) {
    push(reinterpret_cast<Value>(&program()->constants().getCidrArray(A)));
    next;
  }
  // }}}
  // {{{ load & store
  instr(LOAD) {
    push(stack_[A]);
    next;
  }

  instr(STORE) { // STORE imm
    stack_[A] = pop();
    next;
  }
  // }}}
  // {{{ numerical
  instr(ILOAD) {
    push(A);
    next;
  }

  instr(NLOAD) {
    push(program()->constants().getInteger(A));
    next;
  }

  instr(NNEG) {
    SP(-1) = -getNumber(-1);
    next;
  }

  instr(NNOT) {
    SP(-1) = ~getNumber(-1);
    next;
  }

  instr(NADD) {
    SP(-2) = getNumber(-2) + getNumber(-1);
    pop();
    next;
  }

  instr(NSUB) {
    SP(-2) = getNumber(-2) - getNumber(-1);
    pop();
    next;
  }

  instr(NMUL) {
    SP(-2) = getNumber(-2) * getNumber(-1);
    pop();
    next;
  }

  instr(NDIV) {
    SP(-2) = getNumber(-2) / getNumber(-1);
    pop();
    next;
  }

  instr(NREM) {
    SP(-2) = getNumber(-2) % getNumber(-1);
    pop();
    next;
  }

  instr(NSHL) {
    SP(-2) = getNumber(-2) << getNumber(-1);
    pop();
    next;
  }

  instr(NSHR) {
    SP(-2) = getNumber(-2) >> getNumber(-1);
    pop();
    next;
  }

  instr(NPOW) {
    SP(-2) = powl(getNumber(-2), getNumber(-1));
    pop();
    next;
  }

  instr(NAND) {
    SP(-2) = getNumber(-2) & getNumber(-1);
    pop();
    next;
  }

  instr(NOR) {
    SP(-2) = getNumber(-2) | getNumber(-1);
    pop();
    next;
  }

  instr(NXOR) {
    SP(-2) = getNumber(-2) ^ getNumber(-1);
    pop();
    next;
  }

  instr(NCMPZ) {
    SP(-1) = getNumber(-1) == 0;
    next;
  }

  instr(NCMPEQ) {
    SP(-2) = getNumber(-2) == getNumber(-1);
    pop();
    next;
  }

  instr(NCMPNE) {
    SP(-2) = getNumber(-2) != getNumber(-1);
    pop();
    next;
  }

  instr(NCMPLE) {
    SP(-2) = getNumber(-2) <= getNumber(-1);
    pop();
    next;
  }

  instr(NCMPGE) {
    SP(-2) = getNumber(-2) >= getNumber(-1);
    pop();
    next;
  }

  instr(NCMPLT) {
    SP(-2) = getNumber(-2) < getNumber(-1);
    pop();
    next;
  }

  instr(NCMPGT) {
    SP(-2) = getNumber(-2) > getNumber(-1);
    pop();
    next;
  }
  // }}}
  // {{{ boolean
  instr(BNOT) {
    SP(-1) = !getNumber(-1);
    next;
  }

  instr(BAND) {
    SP(-2) = getNumber(-2) && getNumber(-1);
    pop();
    next;
  }

  instr(BOR) {
    SP(-2) = getNumber(-2) || getNumber(-1);
    pop();
    next;
  }

  instr(BXOR) {
    SP(-2) = getNumber(-2) ^ getNumber(-1);
    pop();
    next;
  }
  // }}}
  // {{{ string
  instr(SLOAD) {
    push(reinterpret_cast<Value>(&program()->constants().getString(A)));
    next;
  }

  instr(SADD) {
    SP(-2) = (Value) catString(getString(-2), getString(-1));
    pop();
    next;
  }

  instr(SSUBSTR) {
    SP(-2) = (Value) newString(getString(-3).substr(getNumber(-2), getNumber(-1)));
    stack_.discard(2);
    next;
  }

  instr(SCMPEQ) {
    SP(-2) = getString(-2) == getString(-1);
    pop();
    next;
  }

  instr(SCMPNE) {
    SP(-2) = getString(-2) != getString(-1);
    pop();
    next;
  }

  instr(SCMPLE) {
    SP(-2) = getString(-2) <= getString(-1);
    pop();
    next;
  }

  instr(SCMPGE) {
    SP(-2) = getString(-2) >= getString(-1);
    pop();
    next;
  }

  instr(SCMPLT) {
    SP(-2) = getString(-2) < getString(-1);
    pop();
    next;
  }

  instr(SCMPGT) {
    SP(-2) = getString(-2) > getString(-1);
    pop();
    next;
  }

  instr(SCMPBEG) {
    SP(-2) = beginsWith(getString(-2), getString(-1));
    pop();
    next;
  }

  instr(SCMPEND) {
    SP(-2) = endsWith(getString(-2), getString(-1));
    pop();
    next;
  }

  instr(SCONTAINS) {
    SP(-2) = getString(-2).find(getString(-1)) != std::string::npos;
    pop();
    next;
  }

  instr(SLEN) {
    SP(-1) = getString(-1).size();
    next;
  }

  instr(SISEMPTY) {
    SP(-1) = getString(-1).empty();
    next;
  }

  instr(SMATCHEQ) {
    auto target = program()->match(A)->evaluate(popStringPtr(), this);
    jump_to(target);
  }

  instr(SMATCHBEG) {
    auto target = program()->match(A)->evaluate(popStringPtr(), this);
    jump_to(target);
  }

  instr(SMATCHEND) {
    auto target = program()->match(A)->evaluate(popStringPtr(), this);
    jump_to(target);
  }

  instr(SMATCHR) {
    auto target = program()->match(A)->evaluate(popStringPtr(), this);
    jump_to(target);
  }
  // }}}
  // {{{ ipaddr
  instr(PLOAD) {
    push(reinterpret_cast<Value>(&program()->constants().getIPAddress(A)));
    next;
  }

  instr(PCMPEQ) {
    SP(-2) = getIPAddress(-2) == getIPAddress(-2);
    pop();
    next;
  }

  instr(PCMPNE) {
    SP(-2) = getIPAddress(-2) != getIPAddress(-1);
    pop();
    next;
  }

  instr(PINCIDR) {
    const IPAddress& ipaddr = getIPAddress(-2);
    const Cidr& cidr = getCidr(-1);
    SP(-2) = cidr.contains(ipaddr);
    pop();
    next;
  }
  // }}}
  // {{{ cidr
  instr(CLOAD) {
    push(reinterpret_cast<Value>(&program()->constants().getCidr(A)));
    next;
  }
  // }}}
  // {{{ regex
  instr(SREGMATCH) {  // A =~ B
    const util::RegExp& regex = program()->constants().getRegExp(A);
    const FlowString& data = getString(-1);
    const bool result = regex.match(data, regexpContext_.regexMatch());
    SP(-1) = result;
    next;
  }

  instr(SREGGROUP) {
    FlowNumber position = getNumber(-1);
    util::RegExp::Result& rr = *regexpContext_.regexMatch();
    const std::string& match = rr[position];

    SP(-1) = (Value) newString(match);
    next;
  }
  // }}}
  // {{{ conversion
  instr(S2N) {  // A = atoi(B)
    SP(-1) = std::stoi(getString(-1));
    next;
  }

  instr(N2S) {  // A = itoa(B)
    FlowNumber value = getNumber(-1);
    char buf[64];
    if (snprintf(buf, sizeof(buf), "%" PRIi64 "", (int64_t)value) > 0) {
      SP(-1) = (Value) newString(buf);
    } else {
      SP(-1) = (Value) emptyString();
    }
    next;
  }

  instr(P2S) {
    const IPAddress& ipaddr = getIPAddress(-1);
    SP(-1) = (Value) newString(ipaddr.str());
    next;
  }

  instr(C2S) {
    const Cidr& cidr = getCidr(-1);
    SP(-1) = (Value) newString(cidr.str());
    next;
  }

  instr(R2S) {
    const util::RegExp& re = getRegExp(-1);
    SP(-1) = (Value) newString(re.pattern());
    next;
  }
  // }}}
  // {{{ invokation
  instr(CALL) {
    {
      size_t id = A;
      int argc = B;

      incr_pc();
      ip_ = get_pc();

      Params args(this, argc);
      for (int i = 1; i <= argc; i++)
        args.setArg(i, SP(-(argc + 1) + i));

      const Signature& signature =
          handler_->program()->nativeFunction(id)->signature();

      handler_->program()->nativeFunction(id)->invoke(args);

      discard(argc);
      if (signature.returnType() != LiteralType::Void)
        push(args[0]);

      if (state_ == Suspended) {
        FLOW_DEBUG("flow: vm suspended in function. returning (false)");
        return false;
      }
    }
    set_pc(ip_);
    jump;
  }

  instr(HANDLER) {
    {
      size_t id = A;
      int argc = B;

      incr_pc();
      ip_ = get_pc();

      Params args(this, argc);
      for (int i = 1; i <= argc; i++)
        args.setArg(i, SP(-(argc + 1) + i));

      handler_->program()->nativeHandler(id)->invoke(args);
      const bool handled = (bool) args[0];
      discard(argc);

      if (state_ == Suspended) {
        FLOW_DEBUG("flow: vm suspended in handler. returning (false)");
        return false;
      }

      if (handled) {
        state_ = Inactive;
        return true;
      }
    }
    set_pc(ip_);
    jump;
  }
  // }}}

  LOOP_END()
}

}  // namespace xzero::flow
