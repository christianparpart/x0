/* <x0/plugins/debug.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/io/BufferSource.h>
#include <x0/Process.h>

using namespace x0;

/**
 * \ingroup plugins
 * \brief plugin with some debugging/testing helpers
 */
class DebugPlugin :
    public x0d::XzeroPlugin
{
public:
    DebugPlugin(x0d::XzeroDaemon* d, const std::string& name) :
        x0d::XzeroPlugin(d, name)
    {
        mainHandler("debug.slow_response", &DebugPlugin::slowResponse);
        mainHandler("debug.coredump", &DebugPlugin::dumpCore);
        mainHandler("debug.coredump.post", &DebugPlugin::dumpCorePost);
        mainHandler("debug.dump_request_buffers", &DebugPlugin::dumpRequestBuffers);
    }

private:
    bool dumpRequestBuffers(HttpRequest* r, FlowVM::Params& args)
    {
        r->status = HttpStatus::NotFound;

        unsigned long long cid = r->query.toInt();

        for (auto worker: server().workers()) {
            worker->eachConnection([=](HttpConnection* connection) -> bool {
                if (connection->id() == cid) {
                    if (connection->requestBuffer().empty()) {
                        r->status = HttpStatus::NoContent;
                    } else {
                        r->status = HttpStatus::Ok;
                        r->responseHeaders.push_back("Content-Type", "text/plain; charset=utf8");

                        char buf[128];

                        snprintf(buf, sizeof(buf), "%zu", connection->requestBuffer().size());
                        r->responseHeaders.push_back("Content-Length", buf);

                        snprintf(buf, sizeof(buf), "%zu", connection->requestParserOffset());
                        r->responseHeaders.push_back("X-RequestParser-Offset", buf);

                        r->write<BufferSource>(connection->requestBuffer());
                    }
                }
                return true;
            });
        }

        r->finish();

        return true;
    }

    bool dumpCore(x0::HttpRequest* r, x0::FlowVM::Params& args)
    {
        r->status = x0::HttpStatus::Ok;
        r->responseHeaders.push_back("Content-Type", "text/plain; charset=utf8");

        x0::Buffer buf;
        buf << "Dumping core\n";
        r->write<x0::BufferSource>(std::move(buf));

        r->finish();

        x0::Process::dumpCore();

        return true;
    }

    bool dumpCorePost(x0::HttpRequest* r, x0::FlowVM::Params& args)
    {
        r->status = x0::HttpStatus::Ok;
        r->responseHeaders.push_back("Content-Type", "text/plain; charset=utf8");

        x0::Buffer buf;
        buf << "Dumping core\n";
        r->write<x0::BufferSource>(std::move(buf));

        r->finish();

        server().workers()[0]->post<DebugPlugin, &DebugPlugin::_dumpCore>(this);

        return true;
    }

    void _dumpCore()
    {
        x0::Process::dumpCore();
    }

    bool slowResponse(x0::HttpRequest* r, x0::FlowVM::Params& args)
    {
        const unsigned count = 8;
        for (unsigned i = 0; i < count; ++i) {
            x0::Buffer buf;
            buf << "slow response: " << i << "/" << count << "\n";

            if (i)
                sleep(1); // yes! make it slow!

            printf(": %s", buf.c_str());
            r->write<x0::BufferSource>(std::move(buf));
        }
        r->finish();
        return true;
    }
};

X0_EXPORT_PLUGIN_CLASS(DebugPlugin)
