// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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
      output_(file_->createOutputChannel()) {
}

LogFile::~LogFile() {
}

void LogFile::write(Buffer&& message) {
  output_->write(message.data(), message.size());
  output_->flush();
}

void LogFile::cycle() {
  output_ = file_->createOutputChannel();
}
// }}}
template <typename iterator> inline BufferRef getFormatName(iterator& i, iterator e) { // {{{
  // FormatName ::= '{' NAME '}'

  if (i != e && *i == '{') {
    ++i;
  } else {
    RAISE(RuntimeError, "Expected '{' token.");
  }

  iterator beg = i;

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

  return BufferRef(beg, i - beg - 1);
}
// }}}
Buffer formatLog(XzeroContext* cx, const BufferRef& format) { // {{{
  HttpRequest* request = cx->request();
  HttpResponse* response = cx->response();

  Buffer result;

  auto i = format.begin();
  auto e = format.end();

  while (i != e) {
    if (*i != '%') {
      result.push_back(*i);
      ++i;
      continue;
    }

    ++i;

    if (i == e) {
      break;
    }

    switch (*i) {
      case '%':  // %
        result.push_back(*i);
        ++i;
        break;
      case '>': {  // request header %>{name}
        ++i;
        BufferRef fn = getFormatName(i, e);
        std::string value = request->headers().get(fn.str());
        if (!value.empty()) {
          result.push_back(value);
        } else {
          result.push_back('-');
        }
        break;
      }
      case '<': { // response header %<{name}
        ++i;
        BufferRef fn = getFormatName(i, e);
        std::string value = response->headers().get(fn.str());
        if (!value.empty()) {
          result.push_back(value);
        } else {
          result.push_back('-');
        }
        break;
      }
      case 'C': { // request cookie %C{name}
        ++i;
        BufferRef fn = getFormatName(i, e);
        auto cookies = Cookies::parseCookieHeader(request->headers().get("Cookie"));
        std::string value;
        if (Cookies::getCookie(cookies, fn.str(), &value) && !value.empty()) {
          result.push_back(value);
        } else {
          result.push_back('-');
        }
        break;
      }
      case 'c':  // response status code
        result.push_back(static_cast<int>(response->status()));
        ++i;
        break;
      case 'h':  // request vhost
        result.push_back(request->headers().get("Host"));
        ++i;
        break;
      case 'I':  // received bytes (transport level)
        result.push_back(std::to_string(cx->bytesReceived()));
        ++i;
        break;
      case 'm':  // request method
        result.push_back(request->unparsedMethod());
        ++i;
        break;
      case 'O':  // sent bytes (transport level)
        result.push_back(std::to_string(cx->bytesTransmitted()));
        ++i;
        break;
      case 'o':  // sent bytes (response body)
        result.push_back(std::to_string(response->output()->size()));
        ++i;
        break;
      case 'p':  // request path
        result.push_back(request->path());
        ++i;
        break;
      case 'q':  // query args
        result.push_back(request->query());
        ++i;
        break;
      case 'R':  // remote addr
        result.push_back(cx->remoteIP().c_str());
        ++i;
        break;
      case 'r':  // request line
        result.push_back(request->unparsedMethod());
        result.push_back(' ');
        result.push_back(request->unparsedUri());
        result.push_back(' ');
        result.push_back("HTTP/");
        result.push_back(to_string(request->version()));
        ++i;
        break;
      case 'T': {  // request time duration
        TimeSpan duration = cx->duration();
        result.printf("%d.%03d", duration.totalSeconds(), duration.milliseconds());
        ++i;
        break;
      }
      case 't':  // local time
        result.push_back(cx->now().htlog_str());
        ++i;
        break;
      case 'U':  // username
        ++i;
        if (!request->username().empty()) {
          result.push_back(request->username());
        } else {
          result.push_back('-');
        }
        break;
      case 'u':  // request uri
        result.push_back(request->unparsedUri());
        ++i;
        break;
      default:
        RAISE(RuntimeError, "Unknown format identifier.");
    }
  }

  result.push_back('\n');
  return result;
}
// }}}
struct RequestLogger : public CustomData { // {{{
  XzeroContext* context_;

  std::list<std::pair<FlowString /*format*/, LogFile* /*log*/>> targets_;

  RequestLogger(XzeroContext* cx, const FlowString& format, LogFile* log)
      : context_(cx), targets_() {
    addTarget(format, log);
  }

  void addTarget(const FlowString& format, LogFile* log) {
    targets_.push_back(std::make_pair(format, log));
  }

  ~RequestLogger() {
    for (const auto& target: targets_) {
      Buffer line = formatLog(context_, target.first);
      target.second->write(std::move(line));
    }
  }
};  // }}}

AccesslogModule::AccesslogModule(XzeroDaemon* d)
    : XzeroModule(d, "accesslog"),
      formats_(),
      logfiles_() {

  formats_["combined"] =
      "%R - %U [%t] \"%r\" %c %O \"%>{Referer}\" \"%>{User-Agent}\"";

  formats_["main"] =
      "%R - [%t] \"%r\" %c %O \"%>{User-Agent}\" \"%>{Referer}\"";

  setupFunction("accesslog.format", &AccesslogModule::accesslog_format)
      .param<FlowString>("id")
      .param<FlowString>("format");

  mainFunction("accesslog", &AccesslogModule::accesslog_file)
      .param<FlowString>("file")
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

// accesslog(filename, format = "main");
void AccesslogModule::accesslog_file(XzeroContext* cx, Params& args) {
  FlowString filename = args.getString(1);
  FlowString id = args.getString(2);

  Option<FlowString> format = lookupFormat(id);
  if (format.isNone()) {
    logError("x0d",
             "Could not write to accesslog '%*s' with format id '%*s'. %s",
             filename.size(),
             filename.data(),
             id.size(),
             id.data(),
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
  auto i = logfiles_.find(filename.str());
  if (i != logfiles_.end()) {
    return i->second.get();
  }

  std::string path = filename.str();
  std::shared_ptr<File> file = daemon().vfs().getFile(path);
  LogFile* logFile = new LogFile(file);

  logfiles_[path].reset(logFile);
  return logFile;
}

} // namespace x0d
