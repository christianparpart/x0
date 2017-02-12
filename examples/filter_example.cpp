// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroPlugin.h>
#include <xzero/HttpServer.h>
#include <xzero/HttpRequest.h>
#include <xzero/HttpRangeDef.h>
#include <base/io/Filter.h>
#include <base/strutils.h>
#include <base/Types.h>
#include <x0d/sysconfig.h>

using namespace base;
using namespace flow;
using namespace xzero;

// {{{ ExampleFilter
class ExampleFilter : public Filter {
 public:
  enum Mode { IDENTITY, UPPER, LOWER };

 private:
  Mode mode_;

 public:
  explicit ExampleFilter(Mode mode);
  virtual Buffer process(const BufferRef& input);
};

ExampleFilter::ExampleFilter(Mode mode) : mode_(mode) {}

Buffer ExampleFilter::process(const BufferRef& input) {
  Buffer result;

  switch (mode_) {
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
class filter_plugin : public x0d::XzeroPlugin {
 public:
  filter_plugin(x0d::XzeroDaemon* d, const std::string& name)
      : x0d::XzeroPlugin(d, name) {
    mainFunction("example_filter", &filter_plugin::install_filter,
                 flow::FlowType::String);
  }

  void install_filter(HttpRequest* r, flow::vm::Params& args) {
    auto algo = args.getString(1);
    if (equals(algo, "identity"))
      r->outputFilters.push_back(
          std::make_shared<ExampleFilter>(ExampleFilter::IDENTITY));
    else if (equals(algo, "upper"))
      r->outputFilters.push_back(
          std::make_shared<ExampleFilter>(ExampleFilter::UPPER));
    else if (equals(algo, "lower"))
      r->outputFilters.push_back(
          std::make_shared<ExampleFilter>(ExampleFilter::LOWER));
    else {
      log(Severity::error, "Invalid argument value passed.");
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

X0D_EXPORT_PLUGIN(filter)
