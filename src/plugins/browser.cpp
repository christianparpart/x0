// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroPlugin.h>
#include <xzero/HttpServer.h>
#include <xzero/HttpRequest.h>
#include <xzero/HttpHeader.h>
#include <base/strutils.h>
#include <base/Types.h>

#define TRACE(msg...) this->debug(msg)

using namespace xzero;
using namespace flow;
using namespace base;

/**
 * \ingroup plugins
 * \brief example content generator plugin
 */
class BrowserPlugin : public x0d::XzeroPlugin {
 public:
  BrowserPlugin(x0d::XzeroDaemon* d, const std::string& name)
      : x0d::XzeroPlugin(d, name) {
    setupFunction("browser.ancient", &BrowserPlugin::setAncient,
                  flow::FlowType::String);
    setupFunction("browser.modern", &BrowserPlugin::setModern,
                  flow::FlowType::String, flow::FlowType::String);

    mainFunction("browser.is_ancient", &BrowserPlugin::isAncient,
                 flow::FlowType::Boolean);
    mainFunction("browser.is_modern", &BrowserPlugin::isModern,
                 flow::FlowType::Boolean);
  }

  ~BrowserPlugin() {}

 private:
  std::vector<std::string> ancients_;
  std::map<std::string, float> modern_;

  void setAncient(flow::vm::Params& args) {
    std::string ident = args.getString(1).str();

    ancients_.push_back(ident);
  }

  void setModern(flow::vm::Params& args) {
    std::string browser = args.getString(1).str();
    float version = args.getString(2).toFloat();

    modern_[browser] = version;
  }

  void isAncient(HttpRequest* r, flow::vm::Params& args) {
    BufferRef userAgent(r->requestHeader("User-Agent"));

    for (auto& ancient : ancients_) {
      if (userAgent.find(ancient.c_str()) != BufferRef::npos) {
        args.setResult(true);
        return;
      }
    }
    args.setResult(false);
  }

  void isModern(HttpRequest* r, flow::vm::Params& args) {
    BufferRef userAgent(r->requestHeader("User-Agent"));

    for (auto& modern : modern_) {
      std::size_t i = userAgent.find(modern.first.c_str());
      if (i == BufferRef::npos) continue;

      i += modern.first.size();
      if (userAgent[i] != '/')  // expecting '/' as delimiter
        continue;

      float version = userAgent.ref(++i).toFloat();

      if (version < modern.second) continue;

      args.setResult(true);
      return;
    }
    args.setResult(false);
  }
};

X0D_EXPORT_PLUGIN_CLASS(BrowserPlugin)
