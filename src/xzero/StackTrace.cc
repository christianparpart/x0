// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/StackTrace.h>
#include <xzero/Tokenizer.h>
#include <xzero/Buffer.h>
#include <xzero/sysconfig.h>
#include <typeinfo>
#include <memory>
#include <functional>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>

#if defined(XZERO_OS_UNIX)
#include <cxxabi.h>
#endif

#if defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif


#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#if defined(HAVE_EXECINFO_H)
#include <execinfo.h>
#endif

#if defined(HAVE_DLFCN_H)
#include <dlfcn.h>
#endif

namespace xzero {

constexpr size_t MAX_FRAMES {128};
constexpr size_t SKIP_FRAMES {2};

StackTrace::StackTrace() :
#if defined(HAVE_BACKTRACE)
    frames_{SKIP_FRAMES + MAX_FRAMES}
#else
    frames_{}
#endif
{
#if defined(HAVE_BACKTRACE)
  frames_.resize(::backtrace(&frames_[0], SKIP_FRAMES + MAX_FRAMES));
#endif
}

StackTrace::StackTrace(StackTrace&& other)
    : frames_{std::move(other.frames_)} {
}

StackTrace& StackTrace::operator=(StackTrace&& other) {
  frames_ = std::move(other.frames_);
  return *this;
}

StackTrace::StackTrace(const StackTrace& other)
    : frames_{other.frames_} {
}

StackTrace& StackTrace::operator=(const StackTrace& other) {
  frames_ = other.frames_;
  return *this;
}

StackTrace::~StackTrace() {
}

std::string StackTrace::demangleSymbol(const char* symbol) {
#if defined(XZERO_OS_UNIX)
  int status = 0;
  char* demangled = abi::__cxa_demangle(symbol, nullptr, 0, &status);

  if (demangled) {
    std::string result(demangled, strlen(demangled));
    free(demangled);
    return result;
  } else {
    return symbol;
  }
#else
  return symbol;
#endif
}

std::vector<std::string> StackTrace::symbols() const {
  if (empty())
    return {};

#if defined(HAVE_DLFCN_H)
  std::vector<std::string> output;

  for (size_t i = SKIP_FRAMES; i < size(); ++i) {
    Dl_info info;
    if (dladdr(frames_[i], &info)) {
      if (info.dli_sname && *info.dli_sname) {
        output.push_back(demangleSymbol(info.dli_sname));
      } else {
        char buf[512];
        int n = snprintf(
            buf,
            sizeof(buf),
            "%s %p",
            info.dli_fname,
            frames_[i]);
        output.push_back(std::string(buf, n));
      }
    } else if (frames_[i] != nullptr) {
      char buf[512];
      int n = snprintf(buf, sizeof(buf), "%p", frames_[i]);
      output.push_back(std::string(buf, n));
    } else {
      break;
    }
  }

  return output;
#elif defined(HAVE_BACKTRACE_SYMBOLS)
  int frameCount{0};
  char** strings = backtrace_symbols(&frames_[0], frameCount);

  std::vector<std::string> output;
  try {
    output.resize(frameCount);

    for (int i = 0; i < frameCount; ++i) {
      output[i] = strings[i];
    }
  } catch (...) {
    free(strings);
    throw;
  }
  free(strings);
  return output;
#else
  // TODO: windows port
  return {};
#endif
}

}  // namespace xzero
