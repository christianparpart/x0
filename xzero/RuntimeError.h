// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/Status.h>
#include <xzero/StackTrace.h>
#include <iosfwd>
#include <vector>
#include <string>
#include <stdexcept>
#include <system_error>
#include <string.h>
#include <errno.h>

namespace xzero {

class XZERO_BASE_API RuntimeError : public std::system_error {
 public:
  RuntimeError(int ev, const std::error_category& ec);
  RuntimeError(int ev, const std::error_category& ec, const std::string& what);
  explicit RuntimeError(const std::error_code& ec);

  template<typename... Args>
  RuntimeError(int ev, const std::error_category& ec, const char* fmt, Args... args);

  explicit RuntimeError(Status ev);
  RuntimeError(Status ev, const std::string& what);

  template<typename... Args>
  RuntimeError(Status ev, const char* fmt, Args... args);

  ~RuntimeError();

  template<typename T = RuntimeError>
  T setSource(const char* file, int line, const char* fn);
  const char* sourceFile() const { return sourceFile_; }
  int sourceLine() const XZERO_NOEXCEPT { return sourceLine_; }
  const char* functionName() const { return functionName_; }

  // XXX for backwards-compatibility only
  XZERO_DEPRECATED const char* typeName() const;
  XZERO_DEPRECATED bool ofType(Status ev) const;

  bool operator==(Status status) const;
  bool operator!=(Status status) const;

  std::vector<std::string> backtrace() const;

  void debugPrint(std::ostream* os = nullptr) const;

 private:
  static std::string cformat(const char* fmt, ...);

 private:
  const char* sourceFile_;
  int sourceLine_;
  const char* functionName_;
  StackTrace stackTrace_;
};

XZERO_BASE_API void logAndPass(const std::exception& e);
XZERO_BASE_API void logAndAbort(const std::exception& e);

// {{{ inlines
template<typename T>
inline T RuntimeError::setSource(const char* file, int line, const char* fn) {
  sourceFile_ = file;
  sourceLine_ = line;
  functionName_ = fn;
  return T(*this);
}


template<typename... Args>
inline RuntimeError::RuntimeError(int ev, const std::error_category& ec,
                                  const char* fmt, Args... args)
  : RuntimeError(ev, ec, cformat(fmt, args...)) {
}

template<typename... Args>
inline RuntimeError::RuntimeError(
    Status ev,
    const char* fmt,
    Args... args)
  : RuntimeError((int) ev,
                 StatusCategory::get(),
                 cformat(fmt, args...)) {
}

inline RuntimeError::RuntimeError(const std::error_code& ec)
  : RuntimeError(ec.value(), ec.category()) {
}
// }}}

} // namespace xzero

#define EXCEPTION(E, ...)                                                     \
  (E(__VA_ARGS__).setSource<E>(__FILE__, __LINE__, __PRETTY_FUNCTION__))

/**
 * Raises an exception of given type and arguments being passed to the
 * constructor.
 *
 * The exception must derive from RuntimeError, whose additional
 * source information will be initialized after.
 */
#define RAISE_EXCEPTION(E, ...) {                                             \
  throw E(__VA_ARGS__).setSource<E>(__FILE__, __LINE__, __PRETTY_FUNCTION__); \
}

/**
 * Raises an exception of type RuntimeError for given code and category.
 *
 * Also sets source file, line, and calling function.
 *
 * @param code 1st argument is the error value.
 * @param category 2nd argument is the error category.
 * @param what_arg 3rd (optional) argument is the actual error message.
 */
#define RAISE_CATEGORY(Code, ...) {                                           \
  RAISE_EXCEPTION(::xzero::RuntimeError, ((int) Code), __VA_ARGS__);                   \
}

/**
 * Raises an exception of given std::error_code.
 *
 * @see RAISE_EXCEPTION(E, ...)
 */
#define RAISE_ERROR(ErrorCode) {                                              \
  RAISE_EXCEPTION(RuntimeError, (ErrorCode))                                  \
}

/**
 * Raises an exception of given operating system error code.
 *
 * @see RAISE_EXCEPTION(E, ...)
 */
#define RAISE_ERRNO(errno) {                                                  \
  RAISE_CATEGORY(errno, std::system_category());                              \
}


/**
 * Raises an exception of given operating system error code with custom message.
 *
 * @see RAISE_EXCEPTION(E, ...)
 */
#define RAISE_SYSERR(errno, fmt...) {                                         \
  RAISE_EXCEPTION(RuntimeError, ((int) errno), std::system_category(), fmt);  \
}

/**
 * Raises a RuntimeError for error codes of type Status.
 *
 * @param StatusCode must be a member field of Status.
 */
#define RAISE_STATUS(StatusCode)                                              \
  RAISE_CATEGORY((::xzero::Status:: StatusCode), ::xzero::StatusCategory::get())

/**
 * Alias to RAISE_STATUS(StatusCode).
 *
 * @param StatusCode must be a member of the enum class Status.
 */
#define RAISE(...) {                                                          \
  throw (RuntimeError(Status:: __VA_ARGS__).setSource(__FILE__, __LINE__, __PRETTY_FUNCTION__));  \
}
  //RAISE_EXCEPTION(RuntimeError, (int) Status:: __VA_ARGS__);

/**
 * Raises an exception on given evaluated expression conditional.
 */
#define BUG_ON(cond) {                                                        \
  if (unlikely(cond)) {                                                       \
    RAISE_EXCEPTION(RuntimeError,                                             \
        (int) Status::InternalError,                                          \
        StatusCategory::get(),                                                \
        "BUG ON: (" #cond ")");                                               \
  }                                                                           \
}
