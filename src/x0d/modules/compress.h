// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/XzeroModule.h>
#include <xzero/http/HttpOutputCompressor.h>
#include <string>

namespace x0d {

class CompressModule : public XzeroModule {
 public:
  explicit CompressModule(XzeroDaemon* d);
  ~CompressModule();

 private:
  // setup properties
  void setup_types(Params& args);
  void setup_level(Params& args);
  void setup_minsize(Params& args);
  void setup_maxsize(Params& args);

  //void postProcess(XzeroContext* cx, Params& args);
  void postProcess(xzero::http::HttpRequest* request, xzero::http::HttpResponse* response);

 private:
  xzero::http::HttpOutputCompressor outputCompressor_;
};

} // namespace x0d
