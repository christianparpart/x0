// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/vm/Runner.h>
#include <xzero-flow/vm/Params.h>
#include <xzero-flow/vm/NativeCallback.h>
#include <xzero-flow/vm/Handler.h>
#include <xzero-flow/vm/Program.h>
#include <xzero-flow/vm/Match.h>
#include <xzero-flow/vm/Instruction.h>
#include <xzero/StringUtil.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>
#include <vector>
#include <utility>
#include <memory>
#include <new>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <inttypes.h>

#if 0 // !defined(NDEBUG)
#define TRACE(msg...) logTrace("vm", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

namespace xzero::flow::vm {

// {{{ VM helper preprocessor definitions
#define OP opcode((Instruction) * pc)
#define A operandA((Instruction) * pc)
#define B operandB((Instruction) * pc)
#define C operandC((Instruction) * pc)

#define SP(i) stack_[stack_.size() - (i)]
// #define X SP(0)
// #define Y SP(-1)
// #define Z SP(-2)

#define toString(R)     (*(FlowString*)stack_[R])
#define toStringPtr(R)  ((FlowString*)stack_[R])
#define toIPAddress(R)  (*(IPAddress*)stack_[R])
#define toCidr(R)       (*(Cidr*)stack_[R])
#define toCidrPtr(R)    ((Cidr*)stack_[R])
#define toRegExp(R)     (*(RegExp*)stack_[R])
#define toNumber(R)     ((FlowNumber)stack_[R])

#define incr_pc()       \
  do {                  \
    ++pc;               \
  } while (0)

#define jump_to(offset) \
  do {                  \
    set_pc(offset);     \
    jump;               \
  } while (0)

#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
#define instr(name) \
  l_##name : ++pc;  \
  TRACE("$0",    \
        disassemble((Instruction) * pc, (pc - code.data()) / 2));

#define get_pc() ((pc - code.data()) / 2)
#define set_pc(offset)               \
  do {                               \
    pc = code.data() + (offset) * 2; \
  } while (0)
#define jump goto*(void*)*pc
#define next goto*(void*)*++pc
#else
#define instr(name) \
  l_##name : TRACE("$0", disassemble(*pc, pc - code.data()));

#define get_pc() (pc - code.data())
#define set_pc(offset)           \
  do {                           \
    pc = code.data() + (offset); \
  } while (0)
#define jump goto* ops[OP]
#define next goto* ops[opcode(*++pc)]
#endif

// }}}

std::unique_ptr<Runner> Runner::create(std::shared_ptr<Handler> handler) {
  return std::unique_ptr<Runner>(new Runner(handler));
}

static FlowString* t = nullptr;

Runner::Runner(std::shared_ptr<Handler> handler)
    : handler_(handler),
      program_(handler->program()),
      userdata_(nullptr, nullptr),
      regexpContext_(),
      state_(Inactive),
      pc_(0),
      stack_(),
      stringGarbage_() {
  // initialize emptyString()
  t = newString("");
}

Runner::~Runner() {}

FlowString* Runner::newString(const std::string& value) {
  stringGarbage_.emplace_back(value);
  return &stringGarbage_.back();
}

FlowString* Runner::newString(const char* p, size_t n) {
  stringGarbage_.emplace_back(p, n);
  return &stringGarbage_.back();
}

FlowString* Runner::catString(const FlowString& a, const FlowString& b) {
  stringGarbage_.emplace_back(a + b);
  return &stringGarbage_.back();
}

bool Runner::run() {
  assert(state_ == Inactive);
  TRACE("Running handler $0.", handler_->name());

  return loop();
}

void Runner::suspend() {
  assert(state_ == Running);
  TRACE("Suspending handler $0.", handler_->name());

  state_ = Suspended;
}

bool Runner::resume() {
  assert(state_ == Suspended);
  TRACE("Resuming handler $0.", handler_->name());

  return loop();
}

void Runner::rewind() {
  pc_ = 0;
}

bool Runner::loop() {
  state_ = Running;

#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
  auto& code = handler_->directThreadedCode();
#else
  const auto& code = handler_->code();
#endif

// {{{ jump table
#define label(opcode) &&l_##opcode
  static const void* const ops[] = {
      // misc
      label(NOP),

      // control
      label(EXIT),      label(JMP),       label(JN),        label(JZ),

      // copy
      label(MOV),

      // array
      label(ITCONST),   label(STCONST),   label(PTCONST),   label(CTCONST),

      // numerical
      label(ISTORE),    label(NSTORE),    label(NNEG),      label(NNOT),
      label(NADD),      label(NSUB),      label(NMUL),      label(NDIV),
      label(NREM),      label(NSHL),      label(NSHR),      label(NPOW),
      label(NAND),      label(NOR),       label(NXOR),      label(NCMPZ),
      label(NCMPEQ),    label(NCMPNE),    label(NCMPLE),    label(NCMPGE),
      label(NCMPLT),    label(NCMPGT),

      // boolean op
      label(BNOT),      label(BAND),      label(BOR),       label(BXOR),

      // string op
      label(SCONST),    label(SADD),      label(SADDMULTI), label(SSUBSTR),
      label(SCMPEQ),    label(SCMPNE),    label(SCMPLE),    label(SCMPGE),
      label(SCMPLT),    label(SCMPGT),    label(SCMPBEG),   label(SCMPEND),
      label(SCONTAINS), label(SLEN),      label(SISEMPTY),  label(SMATCHEQ),
      label(SMATCHBEG), label(SMATCHEND), label(SMATCHR),

      // ipaddr
      label(PCONST),    label(PCMPEQ),    label(PCMPNE),    label(PINCIDR),

      // cidr
      label(CCONST),

      // regex
      label(SREGMATCH), label(SREGGROUP),

      // conversion
      label(N2S),       label(P2S),       label(C2S),       label(R2S),
      label(S2I),       label(SURLENC),   label(SURLDEC),

      // invokation
      label(CALL),      label(HANDLER), };
// }}}
// {{{ direct threaded code initialization
#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
  if (code.empty()) {
    const auto& source = handler_->code();
    code.resize(source.size() * 2);

    uint64_t* pc = code.data();
    for (size_t i = 0, e = source.size(); i != e; ++i) {
      Instruction instr = source[i];

      *pc++ = (uint64_t)ops[opcode(instr)];
      *pc++ = instr;
    }
  }
// const void** pc = code.data();
#endif
  // }}}

  const auto* pc = code.data();
  set_pc(pc_);

  jump;

  // {{{ misc
  instr(NOP) { next; }
  // }}}
  // {{{ control
  instr(EXIT) {
    state_ = Inactive;
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
  // {{{ copy
  instr(MOV) {
    data_[A] = data_[B];
    next;
  }
  // }}}
  // {{{ array
  instr(ITCONST) {
    data_[A] = reinterpret_cast<Register>(&program()->constants().getIntArray(B));
    next;
  }
  instr(STCONST) {
    data_[A] =
        reinterpret_cast<Register>(&program()->constants().getStringArray(B));
    next;
  }
  instr(PTCONST) {
    data_[A] =
        reinterpret_cast<Register>(&program()->constants().getIPAddressArray(B));
    next;
  }
  instr(CTCONST) {
    data_[A] =
        reinterpret_cast<Register>(&program()->constants().getCidrArray(B));
    next;
  }
  // }}}
  // {{{ numerical
  instr(ISTORE) {
    stack_[A] = stack_[B];
    next;
  }

  instr(NSTORE) {
    stack_[A] = program()->constants().getInteger(B);
    next;
  }

  instr(NNEG) {
    push(-pop());
    next;
  }

  instr(NNOT) {
    push(~pop());
    next;
  }

  instr(NADD) {
    push(pop() + pop());
    next;
  }

  instr(NSUB) {
    auto y = pop();
    auto x = pop();
    push(x - y);
    next;
  }

  instr(NMUL) {
    push(pop() * pop());
    next;
  }

  instr(NDIV) {
    auto y = pop();
    auto x = pop();
    push(x / y);
    next;
  }

  instr(NREM) {
    auto y = pop();
    auto x = pop();
    push(x % y);
    next;
  }

  instr(NSHL) {
    auto y = pop();
    auto x = pop();
    push(x << y);
    next;
  }

  instr(NSHR) {
    auto y = pop();
    auto x = pop();
    push(x >> y);
    next;
  }

  instr(NPOW) {
    auto y = pop();
    auto x = pop();
    push(powl(x, y));
    next;
  }

  instr(NAND) {
    auto y = pop();
    auto x = pop();
    push(x & y);
    next;
  }

  instr(NOR) {
    auto y = pop();
    auto x = pop();
    push(x | y);
    next;
  }

  instr(NXOR) {
    auto y = pop();
    auto x = pop();
    push(x ^ y);
    next;
  }

  instr(NCMPZ) {
    push(pop() == 0);
    next;
  }

  instr(NCMPEQ) {
    push(pop() == pop());
    next;
  }

  instr(NCMPNE) {
    push(pop() != pop());
    next;
  }

  instr(NCMPLE) {
    auto y = pop();
    auto x = pop();
    push(x <= y);
    next;
  }

  instr(NCMPGE) {
    auto y = pop();
    auto x = pop();
    push(x >= y);
    next;
  }

  instr(NCMPLT) {
    auto y = pop();
    auto x = pop();
    push(x < y);
    next;
  }

  instr(NCMPGT) {
    auto y = pop();
    auto x = pop();
    push(x > y);
    next;
  }
  // }}}
  // {{{ boolean
  instr(BNOT) {
    push(!pop());
    next;
  }

  instr(BAND) {
    push(pop() && pop());
    next;
  }

  instr(BOR) {
    push(pop() || pop());
    next;
  }

  instr(BXOR) {
    push(pop() ^ pop());
    next;
  }
  // }}}
  // {{{ string
  instr(SPUSH) {
    push(reinterpret_cast<Value>(&program()->constants().getString(A)));
    next;
  }

  instr(SADD) {
    auto y = toString(pop());
    auto x = toString(pop());
    push((Value) catString(x, y));
    next;
  }

  instr(SSUBSTR) {
    auto z = toNumber(pop());
    auto y = toNumber(pop());
    auto x = toString(pop());
    pushString(newString(x.substr(y, z));
    next;
  }

  instr(SADDMULTI) {
    // TODO: A = concat(B /*rbase*/, C /*count*/)
    next;
  }

  instr(SCMPEQ) {
    auto y = toString(pop());
    auto x = toString(pop());
    push(x == y);
    next;
  }

  instr(SCMPNE) {
    auto y = toString(pop());
    auto x = toString(pop());
    push(x != y);
    next;
  }

  instr(SCMPLE) {
    auto y = toString(pop());
    auto x = toString(pop());
    push(x <= y);
    next;
  }

  instr(SCMPGE) {
    auto y = toString(pop());
    auto x = toString(pop());
    push(x >= y);
    next;
  }

  instr(SCMPLT) {
    auto y = toString(pop());
    auto x = toString(pop());
    push(x < y);
    next;
  }

  instr(SCMPGT) {
    auto y = toString(pop());
    auto x = toString(pop());
    push(x > y);
    next;
  }

  instr(SCMPBEG) {
    auto y = toString(pop());
    auto x = toString(pop());
    push(StringUtil::beginsWith(x, y));
    next;
  }

  instr(SCMPEND) {
    auto y = toString(pop());
    auto x = toString(pop());
    push(StringUtil::endsWith(x, y));
    next;
  }

  instr(SCONTAINS) {
    auto y = toString(pop());
    auto x = toString(pop());
    push(StringUtil::includes(x, y));
    next;
  }

  instr(SLEN) {
    push(toString(pop()).size());
    next;
  }

  instr(SISEMPTY) {
    push(toString(pop()).empty());
    next;
  }

  instr(SMATCHEQ) {
    auto pattern = toStringPtr(pop());
    auto matchID = A;
    auto result = program()->match(matchID)->evaluate(pattern, this);
    jump_to(result);
  }

  instr(SMATCHBEG) {
    auto pattern = toStringPtr(pop());
    auto matchID = A;
    auto result = program()->match(matchID)->evaluate(pattern, this);
    jump_to(result);
  }

  instr(SMATCHEND) {
    auto pattern = toStringPtr(pop());
    auto matchID = A;
    auto result = program()->match(matchID)->evaluate(pattern, this);
    jump_to(result);
  }

  instr(SMATCHR) {
    auto pattern = toStringPtr(pop());
    auto matchID = A;
    auto result = program()->match(matchID)->evaluate(pattern, this);
    jump_to(result);
  }
  // }}}
  // {{{ ipaddr
  instr(PCONST) {
    push(reinterpret_cast<Value>(&program()->constants().getIPAddress(A)));
    next;
  }

  instr(PCMPEQ) {
    push(toIPAddress(pop()) == toIPAddress(pop()));
    next;
  }

  instr(PCMPNE) {
    push(toIPAddress(pop()) != toIPAddress(pop()));
    next;
  }

  instr(PINCIDR) {
    const Cidr& cidr = toCidr(pop());
    const IPAddress& ipaddr = toIPAddress(pop());
    push(cidr.contains(ipaddr));
    next;
  }
  // }}}
  // {{{ cidr
  instr(CCONST) {
    push(reinterpret_cast<Value>(&program()->constants().getCidr(A)));
    next;
  }
  // }}}
  // {{{ regex
  instr(SREGMATCH) {  // A = B =~ C
    auto regex = program()->constants().getRegExp(pop());
    auto data = toString(pop());
    auto result = regex.match(data, regexpContext_.regexMatch());
    push(result);
    next;
  }

  instr(SREGGROUP) {
    FlowNumber position = toNumber(pop());
    RegExp::Result& rr = *regexpContext_.regexMatch();
    const auto& match = rr[position];

    push((Value) newString(match));
    next;
  }
  // }}}
  // {{{ conversion
  instr(S2I) {  // A = atoi(B)
    push(std::stoi(toString(pop())));
    next;
  }

  instr(N2S) {  // A = itoa(B)
    FlowNumber value = pop();
    char buf[64];
    if (snprintf(buf, sizeof(buf), "%" PRIi64 "", (int64_t)value) > 0) {
      pushString(newString(buf));
    } else {
      pushString(emptyString());
    }
    next;
  }

  instr(P2S) {  // A = ip(B).toString()
    const IPAddress& ipaddr = toIPAddress(pop());
    pushString(newString(ipaddr.str()));
    next;
  }

  instr(C2S) {  // A = cidr(B).toString()
    const Cidr& cidr = toCidr(pop());
    pushString(newString(cidr.str()));
    next;
  }

  instr(R2S) {  // A = regex(B).toString()
    const RegExp& re = toRegExp(pop());
    pushString(newString(re.pattern()));
    next;
  }

  instr(SURLENC) {  // A = urlencode(B)
    // TODO
    next;
  }

  instr(SURLDEC) {  // B = urldecode(B)
    // TODO
    next;
  }
  // }}}
  // {{{ invokation
  instr(CALL) {  // IIR
    size_t id = A;
    int argc = B;
    Register* argv = &data_[C];

    Params args(argc, argv, this);

    TRACE("Calling function: $0",
          handler_->program()->nativeFunction(id)->signature());

    incr_pc();
    pc_ = get_pc();

    handler_->program()->nativeFunction(id)->invoke(args);

    if (state_ == Suspended) {
      logDebug("flow", "vm suspended in function. returning (false)");
      return false;
    }

    set_pc(pc_);
    jump;
  }

  instr(HANDLER) {  // IIR
    size_t id = A;
    int argc = B;
    Value* argv = &data_[C];

    incr_pc();
    pc_ = get_pc();

    Params args(argc, argv, this);
    TRACE("Calling handler: $0",
          handler_->program()->nativeHandler(id)->signature());
    handler_->program()->nativeHandler(id)->invoke(args);
    const bool handled = (bool)argv[0];

    if (state_ == Suspended) {
      logDebug("flow", "vm suspended in handler. returning (false)");
      return false;
    }

    if (handled) {
      state_ = Inactive;
      return true;
    }

    set_pc(pc_);
    jump;
  }
  // }}}
}

std::ostream& operator<<(std::ostream& os, Runner::State state) {
  switch (state) {
    case Runner::Inactive:
      return os << "Inactive";
    case Runner::Running:
      return os << "Running";
    case Runner::Suspended:
      return os << "Suspended";
  }
}

std::ostream& operator<<(std::ostream& os, const Runner& vm) {
  os << "{" << vm.state() << "@" << vm.instructionOffset() << "}";
  return os;
}

}  // namespace xzero::flow::vm
