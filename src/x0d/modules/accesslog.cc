// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

/* plugin type: logger
 *
 * description:
 *     Logs incoming requests to a local file.
 *
 * setup API:
 *     void accesslog.format(string format_id, string format);
 *
 * request processing API:
 *     void accesslog(string file, string format = "main");
 */

#include "accesslog.h"

#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/Cookies.h>
#include <xzero/net/IPAddress.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/logging.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unordered_map>
#include <string>
#include <cerrno>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::flow;

namespace x0d {

// {{{ LogFile impl LogFile impl LogFile impl LogFile impl
LogFile::LogFile(std::shared_ptr<File> file)
    : file_(file),
      fd_(file_->createPosixChannel(File::Write | File::Append)) {
}

LogFile::~LogFile() {
}

void LogFile::write(Buffer&& message) {
  FileUtil::write(fd_, message);
}

void LogFile::cycle() {
  fd_ = file_->createPosixChannel(File::Write | File::Append);
}
// }}}
std::string getFormatName(std::string::const_iterator& i, std::string::const_iterator e) { // {{{
  // FormatName ::= '{' NAME '}'

  if (i != e && *i == '{') {
    ++i;
  } else {
    RAISE(RuntimeError, "Expected '{' token.");
  }

  std::string::const_iterator beg = i;

  for (;;) {
    if (i == e) {
      RAISE(RuntimeError, "Expected '}' token.");
    }

    if (*i == '}') {
      ++i;
      break;
    }

    ++i;
  }

  return std::string(beg, i);
}
// }}}
std::string formatLog(XzeroContext* cx, const std::string& format) { // {{{
  HttpRequest* request = cx->masterRequest();
  HttpResponse* response = cx->response();

  std::stringstream result;

  auto i = format.begin();
  auto e = format.end();

  while (i != e) {
    if (*i != '%') {
      result << (char)(*i);
      ++i;
      continue;
    }

    ++i;

    if (i == e) {
      break;
    }

    switch (*i) {
      case '%':  // %
        result << (char)(*i);
        ++i;
        break;
      case '>': {  // request header %>{name}
        ++i;
        auto fn = getFormatName(i, e);
        std::string value = request->getHeader(fn);
        if (!value.empty()) {
          result << value;
        } else {
          result << '-';
        }
        break;
      }
      case '<': { // response header %<{name}
        ++i;
        auto fn = getFormatName(i, e);
        std::string value = response->getHeader(fn);
        if (!value.empty()) {
          result << value;
        } else {
          result << '-';
        }
        break;
      }
      case 'C': { // request cookie %C{name}
        ++i;
        auto fn = getFormatName(i, e);
        auto cookies = Cookies::parseCookieHeader(request->getHeader("Cookie"));
        std::string value;
        if (Cookies::getCookie(cookies, fn, &value) && !value.empty()) {
          result << value;
        } else {
          result << '-';
        }
        break;
      }
      case 'c':  // response status code
        result << static_cast<int>(response->status());
        ++i;
        break;
      case 'h':  // remote addr
        if (request->remoteAddress().isSome())
          result << request->remoteAddress()->ip().c_str();
        else
          result << "none";

        ++i;
        break;
      case 'I':  // received bytes (transport level)
        result << cx->bytesReceived();
        ++i;
        break;
      case 'l':  // identd user name
        result << '-';
        ++i;
        break;
      case 'm':  // request method
        result << request->unparsedMethod();
        ++i;
        break;
      case 'O':  // sent bytes (transport level)
        result << to_string(cx->bytesTransmitted());
        ++i;
        break;
      case 'o':  // sent bytes (response body)
        result << response->contentLength();
        ++i;
        break;
      case 'p':  // request path
        result << request->path();
        ++i;
        break;
      case 'q':  // query string with leading '?' or empty if none
        if (request->query().empty()) {
          result << '?' << request->query();
        }
        ++i;
        break;
      case 'r':  // request line
        result << request->unparsedMethod()
               << ' ' << request->unparsedUri()
               << " HTTP/" << to_string(request->version());
        ++i;
        break;
      case 'T': {  // request time duration
        Duration duration = cx->age();
        char buf[32];

        snprintf(buf, sizeof(buf),
                 "%" PRIu64 ".%03lu",
                 duration.seconds(),
                 duration.milliseconds() % kMillisPerSecond);
        result << buf;
        ++i;
        break;
      }
      case 't': { // local time
        std::time_t ts = UnixTime::now().unixtime();
        struct tm tm;
        if (localtime_r(&ts, &tm) != nullptr) {
          char buf[256];
          ssize_t n = std::strftime(buf, sizeof(buf), "[%d/%b/%Y:%T %z]", &tm);
          if (n != 0) {
            result << std::string(buf, n);
          }
        }
        ++i;
        break;
      }
      case 'U': // URL path (without query string)
        result << request->path();
        ++i;
        break;
      case 'u':  // username
        if (!request->username().empty()) {
          result << request->username();
        } else {
          result << '-';
        }
        ++i;
        break;
      case 'v':  // request vhost
        result << request->getHeader("Host");
        ++i;
        break;
      default:
        RAISE(RuntimeError, "Unknown format identifier.");
    }
  }

  result << '\n';
  return result.str();
}
// }}}
struct RequestLogger : public CustomData { // {{{
  XzeroContext* context_;

  std::list<std::pair<FlowString /*format*/, LogFile* /*log*/>> targets_;
  std::list<std::pair<FlowString /*format*/, LogTarget* /*target*/>> logTargets_;

  explicit RequestLogger(XzeroContext* cx)
      : context_(cx), targets_() {
  }

  RequestLogger(XzeroContext* cx, const FlowString& format, LogFile* log)
      : RequestLogger(cx) {
    addTarget(format, log);
  }

  void addTarget(const FlowString& format, LogFile* log) {
    targets_.emplace_back(std::make_pair(format, log));
  }

  void addLogTarget(const FlowString& format, LogTarget* logTarget) {
    logTargets_.emplace_back(std::make_pair(format, logTarget));
  }

  ~RequestLogger() {
    for (const auto& target: targets_) {
      std::string line = formatLog(context_, target.first);
      target.second->write(std::move(line));
    }

    for (const auto& target: logTargets_) {
      std::string line = formatLog(context_, target.first);
      target.second->log(LogLevel::Info, line);
    }
  }
};  // }}}

AccesslogModule::AccesslogModule(XzeroDaemon* d)
    : XzeroModule(d, "accesslog"),
      formats_(),
      logfiles_() {

  formats_["combined"] =
      "%h %l %u %t \"%r\" %c %O \"%>{Referer}\" \"%>{User-Agent}\"";

  formats_["main"] =
      "%h %l %t \"%r\" %c %O \"%>{User-Agent}\" \"%>{Referer}\"";

  setupFunction("accesslog.format", &AccesslogModule::accesslog_format)
      .param<FlowString>("id")
      .param<FlowString>("format");

  mainFunction("accesslog", &AccesslogModule::accesslog_file)
      .param<FlowString>("file")
      .param<FlowString>("format", "main");

  mainFunction("accesslog.syslog", &AccesslogModule::accesslog_syslog)
      .param<FlowString>("format", "main");

  onCycleLogs(std::bind(&AccesslogModule::onCycle, this));
}

AccesslogModule::~AccesslogModule() {
}

void AccesslogModule::onCycle() {
  for (auto& logfile: logfiles_) {
    logfile.second->cycle();
  }
}

// accesslog.format(literal string id, literal string format);
void AccesslogModule::accesslog_format(Params& args) {
  FlowString id = args.getString(1);
  FlowString format = args.getString(2);
  formats_[id] = format;
}

Option<FlowString> AccesslogModule::lookupFormat(const FlowString& id) const {
  auto i = formats_.find(id);
  if (i != formats_.end()) {
    return i->second;
  }

  return None(); //Error("accesslog format not found.");
}

void AccesslogModule::accesslog_syslog(XzeroContext* cx, Params& args) {
  // TODO: accesslog.syslog()

  FlowString id = args.getString(1);
  Option<FlowString> format = lookupFormat(id);
  if (format.isNone()) {
    cx->logError(
         "Could not write to accesslog.syslog with format id '$0'. $1",
         BufferRef(id.data(), id.size()),
         "Accesslog format not found.");
    return;
  }

  if (auto rl = cx->customData<RequestLogger>(this)) {
    rl->addLogTarget(format.get(), SyslogTarget::get());
  } else {
    cx->setCustomData<RequestLogger>(this, cx);
    rl->addLogTarget(format.get(), SyslogTarget::get());
  }
}

// accesslog(filename, format = "main");
void AccesslogModule::accesslog_file(XzeroContext* cx, Params& args) {
  FlowString filename = args.getString(1);
  FlowString id = args.getString(2);

  Option<FlowString> format = lookupFormat(id);
  if (format.isNone()) {
    cx->logError(
         "Could not write to accesslog '$0' with format id '$1'. $2",
         BufferRef(filename),
         BufferRef(id),
         "Accesslog format not found.");
    return;
  }

  LogFile* logFile = getLogFile(filename);

  if (auto rl = cx->customData<RequestLogger>(this)) {
    rl->addTarget(format.get(), logFile);
  } else {
    cx->setCustomData<RequestLogger>(this, cx, format.get(), logFile);
  }
}

LogFile* AccesslogModule::getLogFile(const FlowString& filename) {
  auto i = logfiles_.find(filename);
  if (i != logfiles_.end()) {
    return i->second.get();
  }

  std::string path = filename;
  std::shared_ptr<File> file = daemon().vfs().getFile(path);

  return (logfiles_[path] = std::make_unique<LogFile>(file)).get();
}

} // namespace x0d
