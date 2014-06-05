/* <x0d/plugins/echo2_example.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpRequest.h>
#include <x0/io/BufferRefSource.h>

using namespace x0;

/**
 * \ingroup plugins
 * \brief echo content generator plugin
 */
class EchoPlugin :
    public x0d::XzeroPlugin
{
public:
    EchoPlugin(x0d::XzeroDaemon* d, const std::string& name) :
        x0d::XzeroPlugin(d, name)
    {
        mainHandler("echo2_example", &EchoPlugin::handleRequest);
    }

    ~EchoPlugin()
    {
    }

private:
    virtual bool handleRequest(HttpRequest* r, FlowVM::Params& args)
    {
        r->status = HttpStatus::Ok;

        if (r->contentAvailable()) {
            r->consumeBody([this, r](std::unique_ptr<Source>&& body) {
                r->write(std::move(body));
                r->finish();
            });
        } else {
            r->write<BufferRefSource>(BufferRef("I'm an HTTP echo-server, dude.\n"));
            r->finish();
        }

        // yes, we are handling this request
        return true;
    }
};

X0_EXPORT_PLUGIN_CLASS(EchoPlugin)
