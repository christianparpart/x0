// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

/*
 * XXX This plugin is a proof-of-concept and by no means complete nor meant for
 * production.
 * XXX It fits my personal needs. that's all.
 * XXX However, I'll make it more worthy as soon as I have more time for stats
 * and alike :)
 */

#include <x0d/XzeroPlugin.h>
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
class RRDFilePlugin : public x0d::XzeroPlugin {
 private:
  std::atomic<std::size_t> numRequests_;
  std::atomic<std::size_t> bytesIn_;
  std::atomic<std::size_t> bytesOut_;

  std::string filename_;
  int step_;

  ev::timer evTimer_;

 public:
  RRDFilePlugin(x0d::XzeroDaemon* d, const std::string& name)
      : x0d::XzeroPlugin(d, name),
        numRequests_(0),
        bytesIn_(0),
        bytesOut_(0),
        filename_(),
        step_(0),
        evTimer_(server().loop()) {
    evTimer_.set<RRDFilePlugin, &RRDFilePlugin::onTimer>(this);

    setupFunction("rrd.filename", &RRDFilePlugin::setup_filename,
                  x0::FlowType::String);
    setupFunction("rrd.step", &RRDFilePlugin::setup_step, x0::FlowType::Number);
    mainHandler("rrd", &RRDFilePlugin::logRequest);
  }

  ~RRDFilePlugin() {}

 private:
  void setup_step(x0::FlowVM::Params& args) {
    step_ = args.getInt(1);

    if (step_) evTimer_.set(step_, step_);

    checkStart();
  }

  void setup_filename(x0::FlowVM::Params& args) {
    filename_ = args.getString(1);

    checkStart();
  }

  void checkStart() {
    if (step_ > 0 && !filename_.empty()) {
      evTimer_.start();
    }
  }

  void onTimer(ev::timer&, int) {
    if (filename_.empty()) return;  // not properly configured

    char format[128];
    snprintf(format, sizeof(format), "N:%zu:%zu:%zu", numRequests_.exchange(0),
             bytesIn_.exchange(0), bytesOut_.exchange(0));

    const char* args[4] = {"update", filename_.c_str(), format, nullptr};

    rrd_clear_error();
    int rv = rrd_update(3, (char**)args);
    if (rv < 0) {
      log(x0::Severity::error, "Could not update RRD statistics: %s",
          rrd_get_error());
    }
  }

  bool logRequest(x0::HttpRequest* r, x0::FlowVM::Params& args) {
    //++ worker().get<local>(this).counter_[filename];
    ++numRequests_;
    return false;
  }
};

X0_EXPORT_PLUGIN_CLASS(RRDFilePlugin)
