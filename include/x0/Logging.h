// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_Logging_h
#define sw_x0_Logging_h

#include <x0/Api.h>
#include <vector>
#include <string>

namespace x0 {

/** This class is to be inherited by classes that need deeper inspections when
 * logging.
 */
class X0_API Logging {
 private:
  static std::vector<char *> env_;

  std::string prefix_;
  std::string className_;
  bool enabled_;

  void updateClassName();
  bool checkEnabled();

 public:
  Logging();
  explicit Logging(const char *prefix, ...);

  void setLoggingPrefix(const char *prefix, ...);
  void setLogging(bool enable);

  const std::string &loggingPrefix() const { return prefix_; }

  void debug(const char *fmt, ...);

 private:
  static void initialize();
  static void finalize();
};

}  // namespace x0

#endif
