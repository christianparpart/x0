/* <x0/plugins/browser.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
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
	public x0::XzeroPlugin
{
public:
	BrowserPlugin(x0::XzeroDaemon* d, const std::string& name) :
		x0::XzeroPlugin(d, name)
	{
		registerSetupFunction<BrowserPlugin, &BrowserPlugin::setAncient>("browser.ancient");
		registerSetupFunction<BrowserPlugin, &BrowserPlugin::setModern>("browser.modern");

		registerFunction<BrowserPlugin, &BrowserPlugin::isAncient>("browser.is_ancient", x0::FlowValue::BOOLEAN);
		registerFunction<BrowserPlugin, &BrowserPlugin::isModern>("browser.is_modern", x0::FlowValue::BOOLEAN);
	}

	~BrowserPlugin()
	{
	}

private:
	std::vector<std::string> ancients_;
	std::map<std::string, float> modern_;

	void setAncient(const x0::FlowParams& args, x0::FlowValue& result)
	{
		std::string ident = args[0].toString();

		ancients_.push_back(ident);
	}

	void setModern(const x0::FlowParams& args, x0::FlowValue& result)
	{
		std::string browser = args[0].toString();
		float version = x0::Buffer(args[1].toString()).toFloat();

		modern_[browser] = version;
	}

	void isAncient(x0::HttpRequest *r, const x0::FlowParams& args, x0::FlowValue& result)
	{
		x0::BufferRef userAgent(r->requestHeader("User-Agent"));

		for (auto& ancient: ancients_) {
			if (userAgent.find(ancient.c_str()) != x0::BufferRef::npos) {
				result.set(true);
				return;
			}
		}
		result.set(false);
	}

	void isModern(x0::HttpRequest *r, const x0::FlowParams& args, x0::FlowValue& result)
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

			result.set(true);
			return;
		}
		result.set(false);
	}
};

X0_EXPORT_PLUGIN_CLASS(BrowserPlugin)
