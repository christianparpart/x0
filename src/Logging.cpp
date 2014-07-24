// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/Logging.h>
#include <systemd/sd-daemon.h>
#include <typeinfo>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace base {

std::vector<char*> Logging::env_;

Logging::Logging() : prefix_(), className_(), enabled_(false) {}

Logging::Logging(const char* prefix, ...)
    : prefix_(), className_(), enabled_(false) {
  char buf[1024];
  va_list va;

  va_start(va, prefix);
  vsnprintf(buf, sizeof(buf), prefix, va);
  va_end(va);

  prefix_ = buf;

  updateClassName();
}

void Logging::initialize() {
  if (!env_.empty()) return;

  const char* env = std::getenv("XZERO_DEBUG");
  if (!env) return;  // no debugging output requested et al

  char* pattern = strdup(env);
  if (!pattern) return;  // insufficient memory

  char* saveptr = nullptr;
  for (char* sp = pattern; true; sp = nullptr) {
    char* token = strtok_r(sp, ":", &saveptr);
    if (token == nullptr) break;  // end of token-list reached

    if (*token) {
      if (char* out = strdup(token)) {
        env_.push_back(out);
      }
    }
  }

  // TODO: use std::sort() here, to be able to use std::binary_search() later
  // then

  free(pattern);

  std::atexit(&Logging::finalize);
}

void Logging::finalize() {
  for (char* p : env_) free(p);

  env_.clear();
}

void Logging::updateClassName() {
  static const char splits[] = "/[(-";

  for (auto i = prefix_.begin(), e = prefix_.end(); i != e; ++i) {
    if (strchr(splits, *i)) {
      className_ = prefix_.substr(0, i - prefix_.begin());
      return;
    }
  }
  className_ = prefix_;
}

bool Logging::checkEnabled() {
  // have we been enabled explicitely?
  if (enabled_) return true;

  // make sure env_ is populated
  if (env_.empty()) initialize();

  // iterate through environment-supplied list of tokens
  for (char* p : env_)
    if (strcmp(p, className_.c_str()) == 0) return true;

  return false;
}

void Logging::setLoggingPrefix(const char* prefix, ...) {
  char buf[1024];
  va_list va;

  va_start(va, prefix);
  vsnprintf(buf, sizeof(buf), prefix, va);
  va_end(va);

  prefix_ = buf;

  updateClassName();
}

void Logging::setLogging(bool enable) { enabled_ = enable; }

void Logging::debug(const char* fmt, ...) {
  if (!checkEnabled()) return;

  char buf[1024];
  va_list va;

  va_start(va, fmt);
  vsnprintf(buf, sizeof(buf), fmt, va);
  va_end(va);

  fprintf(stdout, SD_DEBUG "%s: %s\n", prefix_.c_str(), buf);
  fflush(stdout);
}

}  // namespace base
