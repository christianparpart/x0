/* <x0/plugins/filter_example.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpRangeDef.h>
#include <x0/io/Filter.h>
#include <x0/strutils.h>
#include <x0/Types.h>
#include <x0/sysconfig.h>

// {{{ ExampleFilter
class ExampleFilter :
	public x0::Filter
{
public:
	enum Mode {
		IDENTITY,
		UPPER,
		LOWER
	};

private:
	Mode mode_;

public:
	explicit ExampleFilter(Mode mode);
	virtual x0::Buffer process(const x0::BufferRef& input);
};

ExampleFilter::ExampleFilter(Mode mode) : mode_(mode)
{
}

x0::Buffer ExampleFilter::process(const x0::BufferRef& input)
{
	x0::Buffer result;

	switch (mode_)
	{
		case ExampleFilter::LOWER:
			for (auto i = input.cbegin(), e = input.cend(); i != e; ++i)
				result.push_back(static_cast<char>(std::tolower(*i)));
			break;
		case ExampleFilter::UPPER:
			for (auto i = input.cbegin(), e = input.cend(); i != e; ++i)
				result.push_back(static_cast<char>(std::toupper(*i)));
			break;
		case ExampleFilter::IDENTITY:
		default:
			result.push_back(input);
	}

	return result;
}
// }}}

/**
 * \ingroup plugins
 * \brief ...
 */
class filter_plugin :
	public x0d::XzeroPlugin
{
public:
	filter_plugin(x0d::XzeroDaemon* d, const std::string& name) :
		x0d::XzeroPlugin(d, name)
	{
		registerFunction<filter_plugin, &filter_plugin::install_filter>("example_filter", x0::FlowValue::VOID);
	}

	~filter_plugin() {
	}

	void install_filter(x0::HttpRequest *r, const x0::FlowParams& args, x0::FlowValue& /*result*/)
	{
		if (args.size() != 1) {
			log(x0::Severity::error, "No argument passed.");
			return;
		}

		if (!args[0].isString()) {
			log(x0::Severity::error, "Invalid argument type passed.");
			return;
		}

		if (strcmp(args[0].toString(), "identity") == 0)
			r->outputFilters.push_back(std::make_shared<ExampleFilter>(ExampleFilter::IDENTITY));
		else if (strcmp(args[0].toString(), "upper") == 0)
			r->outputFilters.push_back(std::make_shared<ExampleFilter>(ExampleFilter::UPPER));
		else if (strcmp(args[0].toString(), "lower") == 0)
			r->outputFilters.push_back(std::make_shared<ExampleFilter>(ExampleFilter::LOWER));
		else {
			log(x0::Severity::error, "Invalid argument value passed.");
			return;
		}

		r->responseHeaders.push_back("Content-Encoding", "filter_example");

		// response might change according to Accept-Encoding
		if (!r->responseHeaders.contains("Vary"))
			r->responseHeaders.push_back("Vary", "Accept-Encoding");
		else
			r->responseHeaders["Vary"] += ",Accept-Encoding";

		// removing content-length implicitely enables chunked encoding
		r->responseHeaders.remove("Content-Length");
	}
};

X0_EXPORT_PLUGIN(filter)
