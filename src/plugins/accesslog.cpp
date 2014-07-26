// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
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
 *     void accesslog.syslog(string format = "main");
 */

#include <x0d/XzeroPlugin.h>
#include <xzero/HttpServer.h>
#include <xzero/HttpRequest.h>
#include <xzero/HttpHeader.h>
#include <base/io/SyslogSink.h>
#include <base/LogFile.h>
#include <base/Logger.h>
#include <base/strutils.h>
#include <base/Try.h>
#include <base/Types.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(HAVE_SYSLOG_H)
#include <syslog.h>
#endif

#include <unordered_map>
#include <string>
#include <cerrno>

using namespace xzero;
using namespace flow;
using namespace base;

template <typename iterator>
inline Try<BufferRef> getFormatName(iterator& i, iterator e) {
  // FormatName ::= '{' NAME '}'

  if (i != e && *i == '{') {
    ++i;
  } else {
    return Error("Expected '{' token.");
  }

  iterator beg = i;

  for (;;) {
    if (i == e) {
      return Error("Expected '}' token.");
    }

    if (*i == '}') {
      ++i;
      break;
    }

    ++i;
  }

  return BufferRef(beg, i - beg - 1);
}

Try<Buffer> formatLog(HttpRequest* r, const BufferRef& format)  // {{{
{
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
        if (Try<BufferRef> fn = getFormatName(i, e)) {
          BufferRef value = r->requestHeader(fn.get());
          if (value) {
            result.push_back(r->requestHeader(fn.get()));
          } else {
            result.push_back('-');
          }
        } else {
          return Error(fn.errorMessage());
        }
        break;
      }
      case '<':  // response header %<{name}
        ++i;
        if (Try<BufferRef> fn = getFormatName(i, e)) {
          if (auto header = r->responseHeaders.findHeader(fn.get().str())) {
            result.push_back(header->value);
          } else {
            result.push_back('-');
          }
        } else {
          return Error(fn.errorMessage());
        }
        break;
      case 'C':  // request cookie %C{name}
        ++i;
        if (Try<BufferRef> fn = getFormatName(i, e)) {
          std::string value = r->cookie(fn.get().str());
          if (!value.empty()) {
            result.push_back(value);
          } else {
            result.push_back('-');
          }
        } else {
          return Error(fn.errorMessage());
        }
        break;
      case 'c':  // response status code
        result.push_back(static_cast<int>(r->status));
        ++i;
        break;
      case 'h':  // request vhost
        result.push_back(r->hostname);
        ++i;
        break;
      case 'I':  // received bytes (transport level)
        // TODO
        result.push_back("-");
        ++i;
        break;
      case 'm':  // request method
        result.push_back(r->method);
        ++i;
        break;
      case 'O':  // sent bytes (transport level)
        result.push_back(r->bytesTransmitted());
        ++i;
        break;
      case 'o':  // sent bytes (response body)
        // TODO
        result.push_back('-');
        ++i;
        break;
      case 'p':  // request path
        result.push_back(r->path);
        ++i;
        break;
      case 'q':  // query args
        result.push_back(r->query);
        ++i;
        break;
      case 'R':  // remote addr
        result.push_back(r->connection.remoteIP().c_str());
        ++i;
        break;
      case 'r':  // request line
        result.push_back(r->method);
        result.push_back(' ');
        result.push_back(r->unparsedUri);
        result.push_back(' ');
        result.push_back("HTTP/");
        result.push_back(r->httpVersionMajor);
        result.push_back('.');
        result.push_back(r->httpVersionMinor);
        ++i;
        break;
      case 'T': {  // request time duration
        TimeSpan duration = r->duration();
        result.printf("%d.%03d", duration.totalSeconds(),
                      duration.milliseconds());
        ++i;
        break;
      }
      case 't':  // local time
        result.push_back(r->connection.worker().now().htlog_str());
        ++i;
        break;
      case 'U':  // username
        ++i;
        if (!r->username.empty()) {
          result.push_back(r->username);
        } else {
          result.push_back('-');
        }
        break;
      case 'u':  // request uri
        result.push_back(r->unparsedUri);
        ++i;
        break;
      default:
        return Error("Unknown format identifier.");
    }
  }

  result.push_back('\n');
  return result;
}  // }}}

struct RequestLogger  // {{{
    : public CustomData {
  HttpRequest* request_;
  std::list<std::pair<FlowString /*format*/, Sink* /*log*/>> targets_;

  RequestLogger(HttpRequest* r, const FlowString& format, Sink* log)
      : request_(r), targets_() {
    addTarget(format, log);
  }

  void addTarget(const FlowString& format, Sink* log) {
    targets_.push_back(std::make_pair(format, log));
  }

  ~RequestLogger() {
    for (const auto& target : targets_) {
      Try<Buffer> result = formatLog(request_, target.first);

      if (result.isError()) {
        request_->log(Severity::error, "Accesslog format error. %s",
                      result.errorMessage());
      } else if (target.second->write(result.get().c_str(),
                                      result.get().size()) <
                 static_cast<ssize_t>(result.get().size())) {
        request_->log(Severity::error,
                      "Could not write to accesslog target. %s",
                      strerror(errno));
      }
    }
  }

  inline std::string hostname(HttpRequest* r) {
    std::string name = r->connection.remoteIP().str();
    return !name.empty() ? name : "-";
  }

  inline std::string username(HttpRequest* r) {
    return !r->username.empty() ? r->username : "-";
  }

  inline std::string request_line(HttpRequest* r) {
    Buffer buf;

    buf << r->method << ' ' << r->unparsedUri << " HTTP/" << r->httpVersionMajor
        << '.' << r->httpVersionMinor;

    return buf.str();
  }

  inline std::string getheader(const HttpRequest* r,
                               const std::string& name) {
    BufferRef value(r->requestHeader(name));
    return !value.empty() ? value.str() : "-";
  }
};  // }}}

/**
 * \ingroup plugins
 * \brief implements an accesslog log facility - in spirit of "combined" mode of
 * apache's accesslog logs.
 */
class AccesslogPlugin : public x0d::XzeroPlugin {
 private:
  typedef std::unordered_map<std::string, std::shared_ptr<LogFile>> LogMap;

#if defined(HAVE_SYSLOG_H)
  SyslogSink syslogSink_;
#endif

  std::unordered_map<FlowString, FlowString> formats_;
  LogMap logfiles_;  // map of file's name-to-fd

 public:
  AccesslogPlugin(x0d::XzeroDaemon* d, const std::string& name)
      : x0d::XzeroPlugin(d, name),
#if defined(HAVE_SYSLOG_H)
        syslogSink_(LOG_INFO),
#endif
        formats_(),
        logfiles_() {
    formats_["combined"] =
        "%R - %U [%t] \"%r\" %c %O \"%>{Referer}\" \"%>{User-Agent}\"";
    formats_["main"] =
        "%R - [%t] \"%r\" %c %O \"%>{User-Agent}\" \"%>{Referer}\"";

    setupFunction("accesslog.format", &AccesslogPlugin::accesslog_format)
        .param<FlowString>("id")
        .param<FlowString>("format");

    mainFunction("accesslog", &AccesslogPlugin::accesslog_file)
        .param<FlowString>("file")
        .param<FlowString>("format", "main");

    mainFunction("accesslog.syslog", &AccesslogPlugin::accesslog_syslog)
        .param<FlowString>("format", "main");
  }

  ~AccesslogPlugin() { clear(); }

  virtual void cycleLogs() {
    for (auto& i : logfiles_) {
      i.second->cycle();
    }
  }

  void clear() { logfiles_.clear(); }

 private:
  // accesslog.format(literal string id, literal string format);
  void accesslog_format(FlowVM::Params& args) {
    FlowString id = args.getString(1);
    FlowString format = args.getString(2);
    formats_[id] = format;
  }

  // accesslog(filename, format = "main");
  void accesslog_file(HttpRequest* r, flow::FlowVM::Params& args) {
    std::string filename(args.getString(1).str());
    FlowString id = args.getString(2);

    Try<FlowString> format = lookupFormat(id);
    if (format.isError()) {
      r->log(Severity::error,
             "Could not write to accesslog '%s' with format id '%s'. %s",
             filename.c_str(), id.str().c_str(), format.errorMessage());
      return;
    }

    LogFile* logFile = getLogFile(filename);

    if (auto rl = r->customData<RequestLogger>(this)) {
      rl->addTarget(format.get(), logFile);
    } else {
      r->setCustomData<RequestLogger>(this, r, format.get(), logFile);
    }
  }

  void accesslog_syslog(HttpRequest* r, flow::FlowVM::Params& args) {
#if defined(HAVE_SYSLOG_H)
    FlowString id = args.getString(1);

    Try<FlowString> format = lookupFormat(id);
    if (format.isError()) {
      r->log(Severity::error,
             "Could not write to accesslog (syslog) with format id '%s'. %s",
             id.str().c_str(), format.errorMessage());
      return;
    }

    if (auto rl = r->customData<RequestLogger>(this)) {
      rl->addTarget(format.get(), &syslogSink_);
    } else {
      r->setCustomData<RequestLogger>(this, r, format.get(), &syslogSink_);
    }
#endif
  }

  Try<FlowString> lookupFormat(const FlowString& id) const {
    auto i = formats_.find(id);
    if (i != formats_.end()) {
      return i->second;
    }

    return Error("accesslog format not found.");
  }

  LogFile* getLogFile(const FlowString& filename) {
    auto i = logfiles_.find(filename.str());
    if (i != logfiles_.end()) {
      return i->second.get();
    }

    auto fileSink = new LogFile(filename.str());
    logfiles_[filename.str()].reset(fileSink);
    return fileSink;
  }
};

X0D_EXPORT_PLUGIN_CLASS(AccesslogPlugin)
