// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpConnection.h>
#include <x0/http/HttpHeader.h>
#include <x0/io/BufferSource.h>
#include <x0/JsonWriter.h>
#include <x0/TimeSpan.h>
#include <x0/strutils.h>
#include <x0/Types.h>

#include <cstring>
#include <cerrno>
#include <cstddef>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define TRACE(msg...) DEBUG("status: " msg)

using namespace x0;

namespace x0 {
JsonWriter& operator<<(JsonWriter& json, double value) {
  json.buffer().printf("%.2f", value);
  json.postValue();
  return json;
}
}

struct Stats : public x0::CustomData {
  unsigned long long connectionsAccepted;
  unsigned long long requestsAccepted;
  unsigned long long requestsProcessed;
  long long active;
  long long connectionStates[6];

  Stats()
      : connectionsAccepted(0),
        requestsAccepted(0),
        requestsProcessed(0),
        active(0),
        connectionStates() {
    for (auto& field : connectionStates) {
      field = 0;
    }
  }

  ~Stats() {}

  long long& reading() {
    return connectionStates[HttpConnection::ReadingRequest];
  }
  long long& writing() {
    return connectionStates[HttpConnection::SendingReply];
  }
  long long& waiting() {
    return connectionStates[HttpConnection::KeepAliveRead];
  }

  Stats& operator+=(const Stats& s) {
    connectionsAccepted += s.connectionsAccepted;
    requestsAccepted += s.requestsAccepted;
    requestsProcessed += s.requestsProcessed;

    for (size_t i = 0; i < sizeof(connectionStates) / sizeof(*connectionStates);
         ++i) {
      connectionStates[i] += s.connectionStates[i];
    }

    return *this;
  }
};

/**
 * \ingroup plugins
 * \brief example content generator plugin
 */
class StatusPlugin : public x0d::XzeroPlugin {
 private:
  std::list<x0::HttpConnection*> connections_;

  Stats historical_;

 public:
  StatusPlugin(x0d::XzeroDaemon* d, const std::string& name)
      : x0d::XzeroPlugin(d, name) {
    mainHandler("status", &StatusPlugin::handleRequest);
    mainHandler("status.nginx_compat", &StatusPlugin::nginx_compat);
    mainHandler("status.api", &StatusPlugin::status_api);

    onWorkerSpawn([=](x0::HttpWorker* worker) {
      worker->setCustomData<Stats>(this);
    });

    onWorkerUnspawn([=](x0::HttpWorker* worker) {
      // XXX stats' active/reading/writing should be all zero at this point
      // already.
      auto stats = worker->customData<Stats>(this);
      historical_ += *stats;
      worker->clearCustomData(this);
    });

    onConnectionOpen([=](x0::HttpConnection* connection) {
      auto stats = connection->worker().customData<Stats>(this);
      ++stats->connectionsAccepted;
      ++stats->active;
    });

    onConnectionStateChanged([=](x0::HttpConnection* connection,
                                 x0::HttpConnection::State lastState) {
      auto stats = connection->worker().customData<Stats>(this);
      --stats->connectionStates[static_cast<size_t>(lastState)];
      ++stats->connectionStates[static_cast<size_t>(connection->state())];
    });

    onConnectionClose([=](x0::HttpConnection* connection) {
      auto stats = connection->worker().customData<Stats>(this);
      --stats->connectionStates[static_cast<size_t>(connection->state())];
      --stats->active;
    });

    onPreProcess([=](x0::HttpRequest* r) {
      auto stats = r->connection.worker().customData<Stats>(this);
      ++stats->requestsAccepted;
    });

    onPostProcess([=](x0::HttpRequest* r) {
      auto stats = r->connection.worker().customData<Stats>(this);
      ++stats->requestsProcessed;
    });
  }

  ~StatusPlugin() {}

 private:
  bool status_api(x0::HttpRequest* r, x0::FlowVM::Params& args) {
    Buffer buf;
    JsonWriter json(buf);

    writeJSON(json);
    buf.push_back('\n');

    char slen[32];
    snprintf(slen, sizeof(slen), "%zu", buf.size());
    r->responseHeaders.overwrite("Content-Length", slen);
    r->responseHeaders.push_back("Content-Type", "application/json");
    r->responseHeaders.push_back("Access-Control-Allow-Origin", "*");
    r->responseHeaders.push_back("Cache-Control", "no-cache");
    r->write<BufferSource>(buf);

    r->finish();

    return true;
  }

  void writeJSON(JsonWriter& response) {
    Stats sum;
    double r1 = 0, r5 = 0, r15 = 0;

    for (std::size_t i = 0, e = server().workers().size(); i != e; ++i) {
      const HttpWorker* w = server().workers()[i];
      const Stats* stats = w->customData<Stats>(this);
      sum += *stats;
      w->fetchPerformanceCounts(&r1, &r5, &r15);
    }

    response.beginObject()
        .name("software-name")("x0d")
        .name("software-version")(PACKAGE_VERSION)
        .name("process-generation")(server().generation())
        .name("process-uptime")(TimeSpan(server().uptime()).str())
        .name("thread-count")(server().workers().size())
        .beginObject("connections")
        .name("accepted")(sum.connectionsAccepted)
        .name("active")(sum.active)
        .name("reading")(sum.reading())
        .name("writing")(sum.writing())
        .name("waiting")(sum.waiting())
        .endObject()
        .beginObject("requests")
        .name("handled")(sum.requestsProcessed)
        .beginObject("load-avg")
        .name("m1")(r1)
        .name("m5")(r5)
        .name("m15")(r15)
        .endObject()
        // TODO: provide response status code counts as key "status-%d"
        .endObject()
        .endObject();
  }

  bool nginx_compat(x0::HttpRequest* r, x0::FlowVM::Params& args) {
    x0::Buffer nginxCompatStatus(1024);
    Stats sum;

    for (std::size_t i = 0, e = server().workers().size(); i != e; ++i) {
      const x0::HttpWorker* w = server().workers()[i];
      const auto stats = w->customData<Stats>(this);
      sum += *stats;
    }

    nginxCompatStatus << "Active connections: " << sum.active << "\n"
                      << "server accepts handled requests\n"
                      << sum.connectionsAccepted << ' ' << sum.requestsAccepted
                      << ' ' << sum.requestsProcessed << "\n"
                      << "Reading: " << sum.reading()
                      << " Writing: " << sum.writing()
                      << " Waiting: " << sum.waiting() << "\n";

    char buf[80];
    snprintf(buf, sizeof(buf), "%zu", nginxCompatStatus.size());
    r->responseHeaders.push_back("Content-Length", (char*)buf);

    r->responseHeaders.push_back("Content-Type", "text/plain");

    r->write<x0::BufferSource>(nginxCompatStatus);

    r->finish();

    return true;
  }

  bool handleRequest(x0::HttpRequest* r, x0::FlowVM::Params& args) {
    // set response status code
    r->status = x0::HttpStatus::Ok;
    r->responseHeaders.push_back("Content-Type", "text/html; charset=utf-8");

    bool debug = true;

    r->write<x0::BufferSource>(createResponseBody(debug));

    r->finish();

    return true;
  }

  // TODO let this method return a movable instead of a full copy
  x0::Buffer createResponseBody(bool debug) {
    /*
     * process uptime
     * number of threads (total / active)
     * cpu load (process / per worker)
     * # requests
     * # connections
     * # requests since start
     */

    x0::TimeSpan uptime(server().uptime());
    std::size_t nconns = 0;
    unsigned long long numTotalRequests = 0;
    unsigned long long numTotalConns = 0;
    double p1 = 0, p5 = 0, p15 = 0;

    for (std::size_t i = 0, e = server().workers().size(); i != e; ++i) {
      const x0::HttpWorker* w = server().workers()[i];
      nconns += w->connectionLoad();
      numTotalRequests += w->requestCount();
      numTotalConns += w->connectionCount();
      w->fetchPerformanceCounts(&p1, &p5, &p15);
    }

    x0::Buffer buf;
    buf << "<html>";
    buf << "<head><title>x0 status page</title>\n";
    buf << "<style>"
           "#conn-table {"
           "border: 1px solid #ccc;"
           "font-size: 11px;"
           "font-family: Helvetica, Arial, freesans, clean, sans-serif;"
           "}"
           "#conn-table th {"
           "border: 1px solid #ccc;"
           "padding-left: 4px;"
           "padding-right: 4px;"
           "}"
           "#conn-table td {"
           "border: 1px solid #ccc;"
           "padding-left: 4px;"
           "padding-right: 4px;"
           "white-space: nowrap;"
           "}"
           "td { vertical-align: top; }"
           ".cid { text-align: right; }"
           ".wid { text-align: right; }"
           ".rn { text-align: right; }"
           ".ip { text-align: center; }"
           ".state { text-align: center; }"
           ".age { text-align: right; }"
           ".idle { text-align: right; }"
           ".read { text-align: right; }"
           ".written { text-align: right; }"
           ".host { text-align: left; }"
           ".method { text-align: center; }"
           ".uri { text-align: left; }"
           ".status { text-align: center; }"
           ".debug { text-align: left; }"
           "</style>";
    buf << "</head>";
    buf << "<body>";
    buf << "<h1>x0 status page</h1>\n";
    buf << "<small><pre>" << server().tag() << "</pre></small>\n";
    buf << "<pre>\n";
    buf << "process uptime: " << uptime << "\n";
    buf << "process generation: " << server().generation() << "\n";

    buf << "average requests per second: ";
    char tmp[80];
    snprintf(tmp, sizeof(tmp), "%.2f, %.2f, %.2f", p1, p5, p15);
    buf << ((char*)tmp) << "\n";

    buf << "# workers: " << server().workers().size() << "\n";
    buf << "# connections: " << nconns << "\n";
    buf << "# total requests: " << numTotalRequests << "\n";
    buf << "# total connections: " << numTotalConns << "\n";
    buf << "</pre>\n";

    buf << "<table border='0' cellspacing='0' cellpadding='0' "
           "id='conn-table'>\n";

    buf << "<th>"
        << "cid"
        << "</th>";
    buf << "<th>"
        << "wid"
        << "</th>";
    buf << "<th>"
        << "r/n"
        << "</th>";
    buf << "<th>"
        << "IP"
        << "</th>";
    buf << "<th>"
        << "state"
        << "</th>";
    buf << "<th>"
        << "age"
        << "</th>";
    buf << "<th>"
        << "idle"
        << "</th>";
    buf << "<th>"
        << "read"
        << "</th>";
    buf << "<th>"
        << "written"
        << "</th>";
    buf << "<th>"
        << "host"
        << "</th>";
    buf << "<th>"
        << "method"
        << "</th>";
    buf << "<th>"
        << "uri"
        << "</th>";
    buf << "<th>"
        << "status"
        << "</th>";

    if (debug)
      buf << "<th>"
          << "debug"
          << "</th>";

    // {{{ FIXME make this one being serialized better
    for (auto w : server().workers()) {
      w->eachConnection([&](x0::HttpConnection* c)->bool {
        dump(buf, c, debug);
        return true;
      });
    }
    // FIXME end. }}}

    buf << "</table>\n";

    buf << "</body></html>\n";

    return buf;
  }

  void dump(x0::Buffer& out, x0::HttpConnection* c, bool debug) {
    out << "<tr>";

    out << "<td class='cid'>" << c->id() << "</td>";
    out << "<td class='wid'>" << c->worker().id() << "</td>";
    out << "<td class='rn'>" << c->requestCount() << "</td>";
    out << "<td class='ip'>" << c->remoteIP().str() << "</td>";

    out << "<td class='state'>" << c->state_str();
    if (c->state() == x0::HttpConnection::ReadingRequest)
      out << " (" << tos(c->parserState()) << ")";
    out << "</td>";

    out << "<td class='age'>"
        << (c->worker().now() - c->socket()->startedAt()).str() << "</td>";
    out << "<td class='idle'>"
        << (c->worker().now() - c->socket()->lastActivityAt()).str() << "</td>";
    out << "<td class='read'>" << c->requestParserOffset() << "/"
        << c->requestBuffer().size() << "</td>";

    const x0::HttpRequest* r = c->request();
    if (r && c->state() != x0::HttpConnection::KeepAliveRead) {
      out << "<td class='written'>" << r->bytesTransmitted() << "</td>";
      out << "<td class='host'>" << sanitize(r->hostname) << "</td>";
      out << "<td class='method'>" << sanitize(r->method) << "</td>";
      out << "<td class='uri'>" << sanitize(r->unparsedUri) << "</td>";
      out << "<td class='status'>" << r->statusStr(r->status) << "</td>";
    } else {
      out << "<td colspan='5'>"
          << "</td>";
    }

    if (debug) {
      out << "<td class='debug'>";
      out << "refs:" << c->refCount() << ", ";

      x0::Buffer inspectBuffer;
      c->socket()->inspect(inspectBuffer);
      if (c->request()) {
        c->request()->inspect(inspectBuffer);
      }

      out << inspectBuffer.replaceAll("\n", "<br/>");

      out << "</td>";
    }

    out << "</tr>\n";
  }

  template <typename T>
  std::string sanitize(const T& value) {
    std::string out;
    char buf[32];
    for (auto i : value) {
      switch (i) {
        case '<':
        case '>':
        case '&':
          snprintf(buf, sizeof(buf), "&#%d;", static_cast<unsigned>(i) & 0xFF);
          out += buf;
          break;
        default:
          out += i;
          break;
      }
    }
    return !out.empty() ? out : "(null)";
  }
};

X0_EXPORT_PLUGIN_CLASS(StatusPlugin)
