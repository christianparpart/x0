// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef x0_Severity_h
#define x0_Severity_h (1)

#include <base/Types.h>
#include <base/Api.h>

#include <string>

namespace base {

//! \addtogroup base
//@{

/**
 * \brief named enum `Severity`, used by logging facility
 * \see logger
 */
struct BASE_API Severity {
  enum {
    trace3 = 0,
    trace2 = 1,
    trace1 = 2,
    debug = 3,
    info = 4,
    notice = 5,
    warning = 6,
    error = 7,
    crit = 8,
    alert = 9,
    emerg = 10,
    warn = warning,
    trace = trace1,
  };

  int value_;

  Severity(int value) : value_(value) {}
  explicit Severity(const std::string& name);
  operator int() const { return value_; }
  const char* c_str() const;

  bool isError() const { return value_ == error; }
  bool isWarning() const { return value_ == warn; }
  bool isInfo() const { return value_ == info; }
  bool isDebug() const { return value_ == debug; }
  bool isTrace() const { return value_ >= trace1; }

  int traceLevel() const { return isTrace() ? 1 + value_ - trace1 : 0; }

  bool set(const char* value);
};

//@}

}  // namespace base

#endif
