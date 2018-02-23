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

#define toString(R)     (*(FlowString*)data_[R])
#define toStringPtr(R)  ((FlowString*)data_[R])
#define toIPAddress(R)  (*(IPAddress*)data_[R])
#define toCidr(R)       (*(Cidr*)data_[R])
#define toCidrPtr(R)    ((Cidr*)data_[R])
#define toRegExp(R)     (*(RegExp*)data_[R])
#define toNumber(R)     ((FlowNumber)data_[R])

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
      label(I2S),       label(P2S),       label(C2S),       label(R2S),
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
    if (npop() != 0) {
      jump_to(A);
    } else {
      next;
    }
  }

  instr(JZ) {
    if (npop() == 0) {
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
    npush(-npop());
    next;
  }

  instr(NNOT) {
    npush(~npop());
    next;
  }

  instr(NADD) {
    npush(npop() + npop());
    next;
  }

  instr(NSUB) {
    auto b = npop();
    auto a = npop();
    npush(a - b);
    next;
  }

  instr(NMUL) {
    npush(npop() * npop());
    next;
  }

  instr(NDIV) {
    auto b = npop();
    auto a = npop();
    npush(a / b);
    next;
  }

  instr(NREM) {
    auto b = npop();
    auto a = npop();
    npush(a % b);
    next;
  }

  instr(NSHL) {
    auto b = npop();
    auto a = npop();
    npush(a << b);
    next;
  }

  instr(NSHR) {
    auto b = npop();
    auto a = npop();
    npush(a >> b);
    next;
  }

  instr(NPOW) {
    auto b = npop();
    auto a = npop();
    npush(powl(a, b));
    next;
  }

  instr(NAND) {
    auto b = npop();
    auto a = npop();
    npush(a & b);
    next;
  }

  instr(NOR) {
    auto b = npop();
    auto a = npop();
    npush(a | b);
    next;
  }

  instr(NXOR) {
    auto b = npop();
    auto a = npop();
    npush(a ^ b);
    next;
  }

  instr(NCMPZ) {
    npush(npop() == 0);
    next;
  }

  instr(NCMPEQ) {
    npush(npop() == npop());
    next;
  }

  instr(NCMPNE) {
    npush(npop() != npop());
    next;
  }

  instr(NCMPLE) {
    auto b = npop();
    auto a = npop();
    npush(a <= b);
    next;
  }

  instr(NCMPGE) {
    auto b = npop();
    auto a = npop();
    npush(a >= b);
    next;
  }

  instr(NCMPLT) {
    auto b = npop();
    auto a = npop();
    npush(a < b);
    next;
  }

  instr(NCMPGT) {
    auto b = npop();
    auto a = npop();
    npush(a > b);
    next;
  }
  // }}}
  // {{{ boolean
  instr(BNOT) {
    npush(!npop());
    next;
  }

  instr(BAND) {
    npush(npop() && npop());
    next;
  }

  instr(BOR) {
    npush(npop() || npop());
    next;
  }

  instr(BXOR) {
    npush(npop() ^ npop());
    next;
  }
  // }}}
  // {{{ string
  instr(SCONST) {  // A = stringConstTable[B]
    npush(reinterpret_cast<Value>(&program()->constants().getString(A)));
    next;
  }

  instr(SADD) {  // A = concat(B, C)
    npush((Value) catString(toString(A), toString(B)));
    next;
  }

  instr(SSUBSTR) {  // substr(A, B /*offset*/, C /*count*/)
    npush((Value) newString(toString(A).substr(stack_[B], stack_[C]));
    next;
  }

  instr(SADDMULTI) {  // TODO: A = concat(B /*rbase*/, C /*count*/)
    next;
  }

  instr(SCMPEQ) {
    auto b = toString(npop());
    auto a = toString(npop());
    npush(a == b);
    next;
  }

  instr(SCMPNE) {
    auto b = toString(npop());
    auto a = toString(npop());
    npush(a != b);
    next;
  }

  instr(SCMPLE) {
    auto b = toString(npop());
    auto a = toString(npop());
    npush(a <= b);
    next;
  }

  instr(SCMPGE) {
    auto b = toString(npop());
    auto a = toString(npop());
    npush(a >= b);
    next;
  }

  instr(SCMPLT) {
    auto b = toString(npop());
    auto a = toString(npop());
    npush(a < b);
    next;
  }

  instr(SCMPGT) {
    auto b = toString(npop());
    auto a = toString(npop());
    npush(a > b);
    next;
  }

  instr(SCMPBEG) {
    auto b = toString(npop());
    auto a = toString(npop());
    npush(StringUtil::beginsWith(a, b));
    next;
  }

  instr(SCMPEND) {
    auto b = toString(npop());
    auto a = toString(npop());
    npush(StringUtil::endsWith(a, b));
    next;
  }

  instr(SCONTAINS) {
    auto b = toString(npop());
    auto a = toString(npop());
    npush(StringUtil::includes(a, b));
    next;
  }

  instr(SLEN) {
    npush(toString(npop()).size());
    next;
  }

  instr(SISEMPTY) {
    npush(toString(npop()).empty());
    next;
  }

  instr(SMATCHEQ) {
    auto pattern = toStringPtr(npop());
    auto matchID = npop();
    auto result = program()->match(matchID)->evaluate(pattern, this);
    jump_to(result);
  }

  instr(SMATCHBEG) {
    auto pattern = toStringPtr(npop());
    auto matchID = npop();
    auto result = program()->match(matchID)->evaluate(pattern, this);
    jump_to(result);
  }

  instr(SMATCHEND) {
    auto pattern = toStringPtr(npop());
    auto matchID = npop();
    auto result = program()->match(matchID)->evaluate(pattern, this);
    jump_to(result);
  }

  instr(SMATCHR) {
    auto pattern = toStringPtr(npop());
    auto matchID = npop();
    auto result = program()->match(matchID)->evaluate(pattern, this);
    jump_to(result);
  }
  // }}}
  // {{{ ipaddr
  instr(PCONST) {
    npush(reinterpret_cast<Value>(&program()->constants().getIPAddress(A)));
    next;
  }

  instr(PCMPEQ) {
    npush(toIPAddress(npop()) == toIPAddress(npop()));
    next;
  }

  instr(PCMPNE) {
    npush(toIPAddress(npop()) != toIPAddress(npop()));
    next;
  }

  instr(PINCIDR) {
    const Cidr& cidr = toCidr(npop());
    const IPAddress& ipaddr = toIPAddress(npop());
    npush(cidr.contains(ipaddr));
    next;
  }
  // }}}
  // {{{ cidr
  instr(CCONST) {
    npush(reinterpret_cast<Value>(&program()->constants().getCidr(A)));
    next;
  }
  // }}}
  // {{{ regex
  instr(SREGMATCH) {  // A = B =~ C
    auto regex = program()->constants().getRegExp(npop());
    auto data = toString(npop());
    auto result = regex.match(data, regexpContext_.regexMatch());
    npush(result);
    next;
  }

  instr(SREGGROUP) {
    FlowNumber position = toNumber(npop());
    RegExp::Result& rr = *regexpContext_.regexMatch();
    const auto& match = rr[position];

    npush((Value) newString(match));
    next;
  }
  // }}}
  // {{{ conversion
  instr(S2I) {  // A = atoi(B)
    npush(std::stoi(toString(npop())));
    next;
  }

  instr(I2S) {  // A = itoa(B)
    auto value = npop();
    char buf[64];
    if (snprintf(buf, sizeof(buf), "%" PRIi64 "", (int64_t)value) > 0) {
      pushString(newString(buf));
    } else {
      pushString(emptyString());
    }
    next;
  }

  instr(P2S) {  // A = ip(B).toString()
    const IPAddress& ipaddr = toIPAddress(npop());
    pushString(newString(ipaddr.str()));
    next;
  }

  instr(C2S) {  // A = cidr(B).toString()
    const Cidr& cidr = toCidr(npop());
    pushString(newString(cidr.str()));
    next;
  }

  instr(R2S) {  // A = regex(B).toString()
    const RegExp& re = toRegExp(npop());
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
