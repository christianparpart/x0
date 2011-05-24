/* <x0/plugins/rrd.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2011 Christian Parpart <trapni@gentoo.org>
 */

/*
 * XXX This plugin is a proof-of-concept and by no means complete nor meant for production.
 * XXX It fits my personal needs. that's all.
 * XXX However, I'll make it more worthy as soon as I have more time for stats and alike :)
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/Types.h>

#include <atomic>
#include <rrd.h>

#define TRACE(msg...) DEBUG("rrd: " msg)

/**
 * \ingroup plugins
 * \brief RRD plugin to keep stats on x0d requests per minute
 */
class rrd_plugin :
	public x0::HttpPlugin
{
private:
	std::atomic<std::size_t> numRequests_;
	std::atomic<std::size_t> bytesIn_;
	std::atomic<std::size_t> bytesOut_;

	std::string filename_;
	int step_;

	ev::timer evTimer_;

public:
	rrd_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name),
		numRequests_(0),
		bytesIn_(0),
		bytesOut_(0),
		filename_(),
		step_(0),
		evTimer_(srv.loop())
	{
		evTimer_.set<rrd_plugin, &rrd_plugin::onTimer>(this);

		registerSetupProperty<rrd_plugin, &rrd_plugin::setup_filename>("rrd.filename", x0::FlowValue::STRING);
		registerSetupProperty<rrd_plugin, &rrd_plugin::setup_step>("rrd.step", x0::FlowValue::NUMBER);
		registerHandler<rrd_plugin, &rrd_plugin::logRequest>("rrd");
	}

	~rrd_plugin()
	{
	}

private:
	void setup_step(const x0::FlowParams& args, x0::FlowValue& result)
	{
		if (args.empty()) {
			result.set(step_);
			return;
		}

		args[0].load(step_);

		if (step_)
			evTimer_.set(step_, step_);

		checkStart();
	}

	void setup_filename(const x0::FlowParams& args, x0::FlowValue& result)
	{
		if (args.empty()) {
			result.set(filename_.c_str());
			return;
		}

		args[0].load(filename_);

		checkStart();
	}

	void checkStart()
	{
		if (step_ > 0 && !filename_.empty()) {
			evTimer_.start();
		}
	}

	void onTimer(ev::timer&, int)
	{
		if (filename_.empty())
			return; // not properly configured

		char format[128];
		snprintf(format, sizeof(format), "N:%ld:%ld:%ld",
				numRequests_.exchange(0),
				bytesIn_.exchange(0),
				bytesOut_.exchange(0));

		const char *args[4] = {
			"update",
			filename_.c_str(),
			format,
			nullptr
		};

		rrd_clear_error();
		int rv = rrd_update(3, (char **) args);
		if (rv < 0) {
			log(x0::Severity::error, "Could not update RRD statistics: %s", rrd_get_error());
		}
	}

	virtual bool logRequest(x0::HttpRequest *r, const x0::FlowParams& args)
	{
		//++ worker().get<local>(this).counter_[filename];
		++numRequests_;
		return false;
	}
};

X0_EXPORT_PLUGIN(rrd)
