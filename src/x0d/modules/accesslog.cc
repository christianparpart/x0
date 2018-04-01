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

#include <fmt/format.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iterator>
#include <unordered_map>
#include <optional>
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
    throw AccesslogFormatError{"Expected '{' token."};
  }

  std::string::const_iterator beg = i;

  for (;;) {
    if (i == e) {
      throw AccesslogFormatError{"Expected '}' token."};
    }

    if (*i == '}') {
      ++i;
      break;
    }

    ++i;
  }

  return std::string(beg, std::prev(i));
}
// }}}
std::string formatLog(Context* cx, const std::string& format) { // {{{
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
        if (request->remoteAddress())
          result << request->remoteAddress()->ip().c_str();
        else
          result << "-";

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
        result << std::to_string(cx->bytesTransmitted());
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
               << " HTTP/" << fmt::format("{}", request->version());
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
        throw AccesslogFormatError{fmt::format("Unknown format identifier '%{}'", *i)};
    }
  }

  result << '\n';
  return result.str();
}
// }}}
void verifyFormat(const std::string& format) { // {{{
  auto i = format.begin();
  auto e = format.end();

  while (i != e) {
    if (*i != '%') {
      ++i;
      continue;
    }

    ++i;

    if (i == e) {
      break;
    }

    switch (*i) {
      case '>': // request header %>{name}
      case '<': // response header %<{name}
      case 'C': { // request cookie %C{name}
        char id = *i;
        ++i;
        if (auto fn = getFormatName(i, e); fn == "") {
          throw AccesslogFormatError{fmt::format(
              "message field for %{}{{}} must not be empty.", id)};
        }
        break;
      }
      case '%': // %
      case 'c': // response status code
      case 'h': // remote addr
      case 'I': // received bytes (transport level)
      case 'l': // identd user name
      case 'm': // request method
      case 'O': // sent bytes (transport level)
      case 'o': // sent bytes (response body)
      case 'p': // request path
      case 'q': // query string with leading '?' or empty if none
      case 'r': // request line
      case 'T': // request time duration
      case 't': // local time
      case 'U': // URL path (without query string)
      case 'u': // username
      case 'v': // request vhost
        break;
      default:
        throw AccesslogFormatError{fmt::format("Unknown format identifier '%{}'", *i)};
    }
  }
}
// }}}
struct RequestLogger : public CustomData { // {{{
  Context* context_;

  std::list<std::pair<FlowString /*format*/, LogFile* /*log*/>> targets_;
  std::list<std::pair<FlowString /*format*/, LogTarget* /*target*/>> logTargets_;
  std::string consoleFormat_;

  explicit RequestLogger(Context* cx)
      : context_(cx), targets_() {
  }

  RequestLogger(Context* cx, const FlowString& format, LogFile* log)
      : RequestLogger(cx) {
    addTarget(format, log);
  }

  void enableConsole(const FlowString& format) {
    consoleFormat_ = format;
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

    if (!consoleFormat_.empty()) {
      std::string line = formatLog(context_, consoleFormat_);
      printf("%s", line.c_str());
    }
  }
};  // }}}

AccesslogModule::AccesslogModule(Daemon* d)
    : Module(d, "accesslog"),
      formats_(),
      logfiles_() {

  formats_["combined"] =
      "%h %l %u %t \"%r\" %c %O \"%>{Referer}\" \"%>{User-Agent}\"";

  formats_["main"] =
      "%h %l %t \"%r\" %c %O \"%>{User-Agent}\" \"%>{Referer}\"";

  setupFunction("accesslog.format", &AccesslogModule::accesslog_format)
      .verifier(&AccesslogModule::accesslog_format_verifier, this)
      .param<FlowString>("id")
      .param<FlowString>("format");

  mainFunction("accesslog.console", &AccesslogModule::accesslog_console)
      .param<FlowString>("format", "main");

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

bool AccesslogModule::accesslog_format_verifier(xzero::flow::Instr* call,
                                                xzero::flow::IRBuilder* builder) {
  if (!dynamic_cast<ConstantString*>(call->operand(1))) {
    throw AccesslogFormatError{"accesslog.format's id parameter must be constant."};
  }

  if (const auto arg = dynamic_cast<ConstantString*>(call->operand(2))) {
    const std::string format = arg->get();
    verifyFormat(format);
  } else {
    throw AccesslogFormatError{"accesslog.format's format parameter must be constant."};
  }

  return true;
}

// accesslog.format(literal string id, literal string format);
void AccesslogModule::accesslog_format(Params& args) {
  FlowString id = args.getString(1);
  FlowString format = args.getString(2);
  formats_[id] = format;
}

std::optional<FlowString> AccesslogModule::lookupFormat(const FlowString& id) const {
  auto i = formats_.find(id);
  if (i != formats_.end()) {
    return i->second;
  }

  return std::nullopt; //Error("accesslog format not found.");
}

void AccesslogModule::accesslog_syslog(Context* cx, Params& args) {
  // TODO: accesslog.syslog()

  FlowString id = args.getString(1);
  std::optional<FlowString> format = lookupFormat(id);
  if (!format) {
    cx->logError(
         "Could not write accesslog to syslog with format id '{}'. {}",
         BufferRef(id.data(), id.size()),
         "Accesslog format not found.");
    return;
  }

  if (auto rl = cx->customData<RequestLogger>(this)) {
    rl->addLogTarget(format.value(), SyslogTarget::get());
  } else {
    cx->setCustomData<RequestLogger>(this, cx);
    rl->addLogTarget(format.value(), SyslogTarget::get());
  }
}

// accesslog.console(format = "main");
void AccesslogModule::accesslog_console(Context* cx, Params& args) {
  FlowString id = args.getString(1);

  std::optional<FlowString> format = lookupFormat(id);
  if (!format) {
    cx->logError(
         "Could not write accesslog to console with format id '{}'. {}",
         id,
         "Accesslog format not found.");
    return;
  }

  if (auto rl = cx->customData<RequestLogger>(this)) {
    rl->enableConsole(format.value());
  } else {
    cx->setCustomData<RequestLogger>(this, cx)->enableConsole(format.value());
  }
}

// accesslog(filename, format = "main");
void AccesslogModule::accesslog_file(Context* cx, Params& args) {
  FlowString filename = args.getString(1);
  FlowString id = args.getString(2);

  std::optional<FlowString> format = lookupFormat(id);
  if (!format) {
    cx->logError(
         "Could not write accesslog to '{}' with format id '{}'. {}",
         filename,
         id,
         "Accesslog format not found.");
    return;
  }

  LogFile* logFile = getLogFile(filename);

  if (auto rl = cx->customData<RequestLogger>(this)) {
    rl->addTarget(format.value(), logFile);
  } else {
    cx->setCustomData<RequestLogger>(this, cx, format.value(), logFile);
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
