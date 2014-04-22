/* <x0d/plugins/status.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpConnection.h>
#include <x0/http/HttpHeader.h>
#include <x0/io/BufferSource.h>
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

struct Stats :
    public x0::CustomData
{
    unsigned long long connectionsAccepted;
    unsigned long long requestsAccepted;
    unsigned long long requestsProcessed;
    long long active;
    long long reading;
    long long writing;

    Stats() :
        connectionsAccepted(0),
        requestsAccepted(0),
        requestsProcessed(0),
        active(0),
        reading(0),
        writing(0)
    {
    }

    ~Stats()
    {
    }

    Stats& operator+=(const Stats& s)
    {
        connectionsAccepted += s.connectionsAccepted;
        requestsAccepted += s.requestsAccepted;
        requestsProcessed += s.requestsProcessed;
        active += s.active;
        reading += s.reading;
        writing += s.writing;

        return *this;
    }
};

/**
 * \ingroup plugins
 * \brief example content generator plugin
 */
class StatusPlugin :
    public x0d::XzeroPlugin
{
private:
    std::list<x0::HttpConnection*> connections_;

    Stats historical_;

    x0::HttpServer::WorkerHook::Connection onWorkerSpawn_;
    x0::HttpServer::WorkerHook::Connection onWorkerUnspawn_;
    x0::HttpServer::ConnectionHook::Connection onConnectionOpen_;
    x0::HttpServer::ConnectionStateHook::Connection onConnectionStateChanged_;
    x0::HttpServer::ConnectionHook::Connection onConnectionClose_;
    x0::HttpServer::RequestHook::Connection onPreProcess_;
    x0::HttpServer::RequestHook::Connection onPostProcess_;

public:
    StatusPlugin(x0d::XzeroDaemon* d, const std::string& name) :
        x0d::XzeroPlugin(d, name)
    {
        mainHandler("status", &StatusPlugin::handleRequest);
        mainHandler("status.nginx_compat", &StatusPlugin::nginx_compat);

        onWorkerSpawn_ = server().onWorkerSpawn.connect<StatusPlugin, &StatusPlugin::onWorkerSpawn>(this);
        onWorkerUnspawn_ = server().onWorkerUnspawn.connect<StatusPlugin, &StatusPlugin::onWorkerUnspawn>(this);

        onConnectionOpen_ = server().onConnectionOpen.connect<StatusPlugin, &StatusPlugin::onConnectionOpen>(this);
        onConnectionStateChanged_ = server().onConnectionStateChanged.connect<StatusPlugin, &StatusPlugin::onConnectionStateChanged>(this);
        onConnectionClose_ = server().onConnectionClose.connect<StatusPlugin, &StatusPlugin::onConnectionClose>(this);

        onPreProcess_ = server().onPreProcess.connect<StatusPlugin, &StatusPlugin::onPreProcess>(this);
        onPostProcess_ = server().onPostProcess.connect<StatusPlugin, &StatusPlugin::onPostProcess>(this);
    }

    ~StatusPlugin()
    {
        server().onWorkerSpawn.disconnect(onWorkerSpawn_);
        server().onWorkerUnspawn.disconnect(onWorkerUnspawn_);

        server().onConnectionOpen.disconnect(onConnectionOpen_);
        server().onConnectionStateChanged.disconnect(onConnectionStateChanged_);
        server().onConnectionClose.disconnect(onConnectionClose_);

        server().onPreProcess.disconnect(onPreProcess_);
        server().onPostProcess.disconnect(onPostProcess_);
    }

private:
    void onWorkerSpawn(x0::HttpWorker* worker) {
        worker->setCustomData<Stats>(this);
    }

    void onWorkerUnspawn(x0::HttpWorker* worker) {
        // XXX stats' active/reading/writing should be all zero at this point already.
        auto stats = worker->customData<Stats>(this);
        historical_ += *stats;
        worker->clearCustomData(this);
    }

    void onConnectionOpen(x0::HttpConnection* connection) {
        auto stats = connection->worker().customData<Stats>(this);
        ++stats->connectionsAccepted;
        ++stats->active;
    }

    void onConnectionStateChanged(x0::HttpConnection* connection, x0::HttpConnection::State lastState) {
        auto stats = connection->worker().customData<Stats>(this);

        switch (lastState) {
            case x0::HttpConnection::ReadingRequest:
                --stats->reading;
                break;
            case x0::HttpConnection::SendingReply:
                --stats->writing;
                break;
            default:
                break;
        }

        switch (connection->state()) {
            case x0::HttpConnection::ReadingRequest:
                ++stats->reading;
                break;
            case x0::HttpConnection::SendingReply:
                ++stats->writing;
                break;
            default:
                break;
        }
    }

    void onConnectionClose(x0::HttpConnection* connection) {
        auto stats = connection->worker().customData<Stats>(this);

        switch (connection->state()) {
            case x0::HttpConnection::ReadingRequest:
                --stats->reading;
                break;
            case x0::HttpConnection::SendingReply:
                --stats->writing;
                break;
            default:
                break;
        }

        --stats->active;
    }

    void onPreProcess(x0::HttpRequest* r) {
        auto stats = r->connection.worker().customData<Stats>(this);
        ++stats->requestsAccepted;
    }

    void onPostProcess(x0::HttpRequest* r) {
        auto stats = r->connection.worker().customData<Stats>(this);
        ++stats->requestsProcessed;
    }

    bool nginx_compat(x0::HttpRequest* r, x0::FlowVM::Params& args)
    {
        x0::Buffer nginxCompatStatus(1024);
        Stats sum;

        for (std::size_t i = 0, e = server().workers().size(); i != e; ++i) {
            const x0::HttpWorker *w = server().workers()[i];
            const auto stats = w->customData<Stats>(this);
            sum += *stats;
        }

        nginxCompatStatus
            << "Active connections: " << sum.active << "\n"
            << "server accepts handled requests\n"
            << sum.connectionsAccepted << ' ' << sum.requestsAccepted << ' ' << sum.requestsProcessed << "\n"
            << "Reading: " << sum.reading << " Writing: " << sum.writing << " Waiting: " << (sum.active - (sum.reading + sum.writing)) << "\n"
            ;

        char buf[80];
        snprintf(buf, sizeof(buf), "%zu", nginxCompatStatus.size());
        r->responseHeaders.push_back("Content-Length", (char *)buf);

        r->responseHeaders.push_back("Content-Type", "text/plain");

        r->write<x0::BufferSource>(nginxCompatStatus);

        r->finish();

        return true;
    }

    bool handleRequest(x0::HttpRequest *r, x0::FlowVM::Params& args)
    {
        // set response status code
        r->status = x0::HttpStatus::Ok;
        r->responseHeaders.push_back("Content-Type", "text/html; charset=utf-8");

        bool debug = true;

        r->write<x0::BufferSource>(createResponseBody(debug));

        r->finish();

        return true;
    }

    // TODO let this method return a movable instead of a full copy
    x0::Buffer createResponseBody(bool debug)
    {
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
            const x0::HttpWorker *w = server().workers()[i];
            nconns += w->connectionLoad();
            numTotalRequests += w->requestCount();
            numTotalConns += w->connectionCount();
            w->fetchPerformanceCounts(&p1, &p5, &p15);
        }

        x0::Buffer buf;
        buf << "<html>";
        buf << "<head><title>x0 status page</title>\n";
        buf <<
            "<style>"
            "#conn-table {"
                "border: 1px solid #ccc;"
                "font-size: 11px;"
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

        buf << "<table border='0' cellspacing='0' cellpadding='0' id='conn-table'>\n";

        buf << "<th>" << "cid" << "</th>";
        buf << "<th>" << "wid" << "</th>";
        buf << "<th>" << "r/n" << "</th>";
        buf << "<th>" << "IP" << "</th>";
        buf << "<th>" << "state" << "</th>";
        buf << "<th>" << "age" << "</th>";
        buf << "<th>" << "idle" << "</th>";
        buf << "<th>" << "read" << "</th>";
        buf << "<th>" << "written" << "</th>";
        buf << "<th>" << "host" << "</th>";
        buf << "<th>" << "method" << "</th>";
        buf << "<th>" << "uri" << "</th>";
        buf << "<th>" << "status" << "</th>";

        if (debug)
            buf << "<th>" << "debug" << "</th>";

        // {{{ FIXME make this one being serialized better
        for (auto w: server().workers()) {
            w->eachConnection([&](x0::HttpConnection* c) -> bool {
                dump(buf, c, debug);
                return true;
            });
        }
        // FIXME end. }}}

        buf << "</table>\n";

        buf << "</body></html>\n";

        return buf;
    }

    void dump(x0::Buffer& out, x0::HttpConnection* c, bool debug)
    {
        out << "<tr>";

        out << "<td class='cid'>" << c->id() << "</td>";
        out << "<td class='wid'>" << c->worker().id() << "</td>";
        out << "<td class='rn'>" << c->requestCount() << "</td>";
        out << "<td class='ip'>" << c->remoteIP().str() << "</td>";

        out << "<td class='state'>" << c->state_str();
        if (c->state() == x0::HttpConnection::ReadingRequest)
            out << " (" << c->state_str() << ")";
        out << "</td>";

        out << "<td class='age'>" << (c->worker().now() - c->socket()->startedAt()).str() << "</td>";
        out << "<td class='idle'>" << (c->worker().now() - c->socket()->lastActivityAt()).str() << "</td>";
        out << "<td class='read'>" << c->requestParserOffset() << "/" << c->requestBufferSize() << "</td>";

        const x0::HttpRequest* r = c->request();
        if (r && c->state() != x0::HttpConnection::KeepAliveRead) {
            out << "<td class='written'>" << r->bytesTransmitted() << "</td>";
            out << "<td class='host'>" << sanitize(r->hostname) << "</td>";
            out << "<td class='method'>" << sanitize(r->method) << "</td>";
            out << "<td class='uri'>" << sanitize(r->unparsedUri) << "</td>";
            out << "<td class='status'>" << r->statusStr(r->status) << "</td>";
        } else {
            out << "<td colspan='5'>" << "</td>";
        }

        if (debug) {
            out << "<td class='debug'>";
            out << "refcount:" << c->refCount() << ", ";
            c->socket()->inspect(out);
            if (c->request()) {
                c->request()->inspect(out);
            }
            out << "</td>";
        }

        out << "</tr>\n";
    }

    template<typename T>
    std::string sanitize(const T& value)
    {
        std::string out;
        char buf[32];
        for (auto i: value) {
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
