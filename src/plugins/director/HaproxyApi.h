// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include "Backend.h"
#include "Director.h"
#include "HealthMonitor.h"

#include <base/Buffer.h>
#include <base/TimeSpan.h>
#include <base/CustomDataMgr.h>
#include <xzero/HttpRequest.h>

class HaproxyApi : public base::CustomData {
 private:
  typedef std::unordered_map<std::string, std::unique_ptr<Director>>
  DirectorMap;

  DirectorMap* directors_;

 public:
  explicit HaproxyApi(DirectorMap* directors);
  ~HaproxyApi();

  void monitor(xzero::HttpRequest* r);
  void stats(xzero::HttpRequest* r, const std::string& prefix);

 private:
  void csv(xzero::HttpRequest* r);
  void buildFrontendCSV(Buffer& buf, Director* director);
};
