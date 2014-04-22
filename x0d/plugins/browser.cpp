/* <x0/plugins/browser.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/strutils.h>
#include <x0/Types.h>

#define TRACE(msg...) this->debug(msg)

/**
 * \ingroup plugins
 * \brief example content generator plugin
 */
class BrowserPlugin :
    public x0d::XzeroPlugin
{
public:
    BrowserPlugin(x0d::XzeroDaemon* d, const std::string& name) :
        x0d::XzeroPlugin(d, name)
    {
        setupFunction("browser.ancient", &BrowserPlugin::setAncient, x0::FlowType::String);
        setupFunction("browser.modern", &BrowserPlugin::setModern, x0::FlowType::String, x0::FlowType::String);

        mainFunction("browser.is_ancient", &BrowserPlugin::isAncient, x0::FlowType::Boolean);
        mainFunction("browser.is_modern", &BrowserPlugin::isModern, x0::FlowType::Boolean);
    }

    ~BrowserPlugin()
    {
    }

private:
    std::vector<std::string> ancients_;
    std::map<std::string, float> modern_;

    void setAncient(x0::FlowVM::Params& args)
    {
        std::string ident = args.getString(1).str();

        ancients_.push_back(ident);
    }

    void setModern(x0::FlowVM::Params& args)
    {
        std::string browser = args.getString(1).str();
        float version = args.getString(2).toFloat();

        modern_[browser] = version;
    }

    void isAncient(x0::HttpRequest *r, x0::FlowVM::Params& args)
    {
        x0::BufferRef userAgent(r->requestHeader("User-Agent"));

        for (auto& ancient: ancients_) {
            if (userAgent.find(ancient.c_str()) != x0::BufferRef::npos) {
                args.setResult(true);
                return;
            }
        }
        args.setResult(false);
    }

    void isModern(x0::HttpRequest *r, x0::FlowVM::Params& args)
    {
        x0::BufferRef userAgent(r->requestHeader("User-Agent"));

        for (auto& modern: modern_) {
            std::size_t i = userAgent.find(modern.first.c_str());
            if (i == x0::BufferRef::npos)
                continue;

            i += modern.first.size();
            if (userAgent[i] != '/') // expecting '/' as delimiter
                continue;

            float version = userAgent.ref(++i).toFloat();

            if (version < modern.second)
                continue;

            args.setResult(true);
            return;
        }
        args.setResult(false);
    }
};

X0_EXPORT_PLUGIN_CLASS(BrowserPlugin)
