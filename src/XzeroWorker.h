// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/TimeSpan.h>
#include <xzero/DateTime.h>
#include <xzero/executor/Scheduler.h>
#include <xzero/logging.h>
#include <memory>
#include <thread>

namespace xzero {
  namespace http {
    class HttpRequest;
    class HttpResponse;
  }
}

namespace x0d {

class XzeroDaemon;

class XzeroWorker {
 public:
  XzeroWorker(XzeroDaemon* d, unsigned id, bool threaded);
  ~XzeroWorker();

  XzeroDaemon& daemon() const;
  unsigned id() const;
  const std::string& name() const;

  xzero::TimeSpan uptime() const;
  xzero::DateTime startupTime() const;
  xzero::DateTime now() const;

  xzero::Scheduler* scheduler() const XZERO_NOEXCEPT;

  void runLoop();

  template <typename... Args> void logError(const char* fmt, Args... args);
  template <typename... Args> void logWarning(const char* fmt, Args... args);
  template <typename... Args> void logInfo(const char* fmt, Args... args);
  template <typename... Args> void logDebug(const char* fmt, Args... args);
  template <typename... Args> void logTrace(const char* fmt, Args... args);

 private:
  void handleRequest(
      xzero::http::HttpRequest* request,
      xzero::http::HttpResponse* response);

 private:
  XzeroDaemon* daemon_;
  unsigned id_;
  std::string name_;
  xzero::DateTime startupTime_;
  xzero::DateTime now_;
  std::unique_ptr<xzero::Scheduler> scheduler_;

  std::thread thread_;
};

// {{{ inlines
inline XzeroDaemon& XzeroWorker::daemon() const {
  return *daemon_;
}

inline unsigned XzeroWorker::id() const {
  return id_;
}

inline const std::string& XzeroWorker::name() const {
  return name_;
}

inline xzero::TimeSpan XzeroWorker::uptime() const {
  return now() - startupTime();
}

inline xzero::DateTime XzeroWorker::startupTime() const {
  return startupTime_;
}

inline xzero::DateTime XzeroWorker::now() const {
  return now_;
}

template <typename... Args>
inline void XzeroWorker::logError(const char* fmt, Args... args) {
  xzero::logError(name().c_str(), fmt, args...);
}

template <typename... Args>
inline void XzeroWorker::logWarning(const char* fmt, Args... args) {
  xzero::logWarning(name().c_str(), fmt, args...);
}

template <typename... Args>
inline void XzeroWorker::logInfo(const char* fmt, Args... args) {
  xzero::logInfo(name().c_str(), fmt, args...);
}

template <typename... Args>
inline void XzeroWorker::logDebug(const char* fmt, Args... args) {
  xzero::logDebug(name().c_str(), fmt, args...);
}

template <typename... Args>
inline void XzeroWorker::logTrace(const char* fmt, Args... args) {
  xzero::logTrace(name().c_str(), fmt, args...);
}

inline xzero::Scheduler* XzeroWorker::scheduler() const XZERO_NOEXCEPT {
  return scheduler_.get();
}
// }}}

} // namespace x0d
