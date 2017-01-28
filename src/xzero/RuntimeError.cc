// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/RuntimeError.h>
#include <xzero/StackTrace.h>
#include <xzero/Tokenizer.h>
#include <xzero/Buffer.h>
#include <xzero/StringUtil.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>

#include <iostream>
#include <typeinfo>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>

#if defined(HAVE_EXECINFO_H)
#include <execinfo.h>
#endif

namespace xzero {

#define MAX_FRAMES 64
#define SKIP_FRAMES 2

void logAndPass(const std::exception& e) {
  logError(typeid(e).name(), e, "unhandled exception");
}

void logAndAbort(const std::exception& e) {
  logAndPass(e);
  abort();
}

RuntimeError::RuntimeError(int ev, const std::error_category& ec)
  : std::system_error(ev, ec),
    sourceFile_(""),
    sourceLine_(0),
    functionName_(""),
    stackTrace_() {
}

RuntimeError::RuntimeError(
    int ev,
    const std::error_category& ec,
    const std::string& what)
  : std::system_error(ev, ec, what),
    sourceFile_(""),
    sourceLine_(0),
    functionName_(""),
    stackTrace_() {
}

RuntimeError::RuntimeError(Status ev)
  : RuntimeError((int) ev, StatusCategory::get()) {
}

RuntimeError::RuntimeError(Status ev, const std::string& what)
  : RuntimeError((int) ev, StatusCategory::get(), what) {
}

RuntimeError::~RuntimeError() {
}

std::vector<std::string> RuntimeError::backtrace() const {
  return stackTrace_.symbols();
}

const char* RuntimeError::typeName() const {
  return typeid(*this).name();
}

bool RuntimeError::ofType(Status ev) const {
  return static_cast<int>(ev) == code().value()
      && &code().category() == &StatusCategory().get();
}

bool RuntimeError::operator==(Status status) const {
  return &code().category() == &StatusCategory().get()
      && static_cast<int>(status) == code().value();
}

bool RuntimeError::operator!=(Status status) const {
  return !(*this == status);
}

void RuntimeError::debugPrint(std::ostream* os) const {
  if (os == nullptr) {
    os = &std::cerr;
  }

  *os << StringUtil::format(
            "$0: $1\n"
            "    in $2\n"
            "    in $3:$4\n",
            typeid(*this).name(),
            what(),
            functionName_,
            sourceFile_,
            sourceLine_);
}

std::string RuntimeError::cformat(const char* fmt, ...) {
  va_list ap;
  char buf[1024];

  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  return std::string(buf, n);
}

} // namespace xzero
