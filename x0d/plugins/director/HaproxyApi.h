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

#include <x0/Buffer.h>
#include <x0/TimeSpan.h>
#include <x0/CustomDataMgr.h>
#include <x0/http/HttpRequest.h>

class HaproxyApi : public x0::CustomData {
 private:
  typedef std::unordered_map<std::string, std::unique_ptr<Director>>
  DirectorMap;

  DirectorMap* directors_;

 public:
  explicit HaproxyApi(DirectorMap* directors);
  ~HaproxyApi();

  void monitor(x0::HttpRequest* r);
  void stats(x0::HttpRequest* r, const std::string& prefix);

 private:
  void csv(x0::HttpRequest* r);
  void buildFrontendCSV(Buffer& buf, Director* director);
};
