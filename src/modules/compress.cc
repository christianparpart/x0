// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "compress.h"
#include <x0d/XzeroModule.h>
#include <x0d/XzeroContext.h>

#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/io/FileUtil.h>
#include <xzero/WallClock.h>
#include <xzero/StringUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/Tokenizer.h>
#include <sstream>

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::flow;

namespace x0d {

CompressModule::CompressModule(XzeroDaemon* d)
    : XzeroModule(d, "compress"),
      outputCompressor_() {

  onPostProcess(
      std::bind(&CompressModule::postProcess, this,
                std::placeholders::_1, std::placeholders::_2));

  setupFunction("compress.types", &CompressModule::setup_types, flow::FlowType::StringArray);
  setupFunction("compress.level", &CompressModule::setup_level, flow::FlowType::Number);
  setupFunction("compress.min", &CompressModule::setup_minsize, flow::FlowType::Number);
  setupFunction("compress.max", &CompressModule::setup_maxsize, flow::FlowType::Number);
}

CompressModule::~CompressModule() {
}

void CompressModule::setup_types(Params& args) {
  const auto& x = args.getStringArray(1);

  for (int i = 0, e = args.size(); i != e; ++i) {
    outputCompressor_.addMimeType(x[i].str());
  }
}

void CompressModule::setup_level(Params& args) {
  outputCompressor_.setCompressionLevel(args.getInt(1));
}

void CompressModule::setup_minsize(Params& args) {
  outputCompressor_.setMinSize(args.getInt(1));
}

void CompressModule::setup_maxsize(Params& args) {
  outputCompressor_.setMaxSize(args.getInt(1));
}

void CompressModule::postProcess(HttpRequest* request, HttpResponse* response) {
  outputCompressor_.postProcess(request, response);
}

} // namespace x0d
