// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include "Backend.h"
#include "Director.h"
#include "HealthMonitor.h"

#include <base/Buffer.h>
#include <base/Duration.h>
#include <base/CustomDataMgr.h>
#include <xzero/HttpRequest.h>

class DirectorPlugin;
class Director;

typedef std::unordered_map<std::string, std::unique_ptr<Director>> DirectorMap;

enum class HttpMethod {
  Unknown,

  // HTTP
  GET,
  PUT,
  POST,
  DELETE,
  CONNECT,

  // WebDAV
  MKCOL,
  MOVE,
  COPY,
  LOCK,
  UNLOCK,
};

class ApiRequest : public base::CustomData {
 private:
  DirectorMap* directors_;
  xzero::HttpRequest* request_;
  HttpMethod method_;
  base::BufferRef path_;
  std::vector<BufferRef> tokens_;
  std::unordered_map<std::string, std::string> args_;
  int errorCount_;

 public:
  ApiRequest(DirectorMap* directors, xzero::HttpRequest* r,
             const base::BufferRef& path);
  ~ApiRequest();

  static bool process(DirectorMap* directors, xzero::HttpRequest* r,
                      const base::BufferRef& path);

 protected:
  Director* findDirector(const base::BufferRef& name);
  bool hasParam(const std::string& key) const;
  bool loadParam(const std::string& key, bool& result);
  bool loadParam(const std::string& key, int& result);
  bool loadParam(const std::string& key, size_t& result);
  bool loadParam(const std::string& key, float& result);
  bool loadParam(const std::string& key, base::Duration& result);
  bool loadParam(const std::string& key, BackendRole& result);
  bool loadParam(const std::string& key, HealthMonitor::Mode& result);
  bool loadParam(const std::string& key, std::string& result);
  bool loadParam(const std::string& key, ClientAbortAction& result);

 private:
  HttpMethod requestMethod() const { return method_; }

  bool run();
  bool process();

  // index
  bool processIndex();
  bool index();

  // director
  bool processDirector();
  bool show(Director* director);
  bool show(Backend* backend);
  bool update(Director* director);
  bool lock(bool enable, Director* director);
  bool destroy(Director* director);
  bool createDirector(const std::string& name);

  // backend
  bool processBackend();
  bool createBackend(Director* director);
  bool update(Backend* backend, Director* director);
  bool lock(bool enable, Backend* backend, Director* director);
  bool destroy(Backend* backend, Director* director);

  // bucket
  bool processBucket();
  void processBucket(Director* director);
  bool createBucket(Director* director);
  void show(RequestShaper::Node* bucket);
  void update(RequestShaper::Node* bucket, Director* director);

  // helper
  static std::vector<base::BufferRef> tokenize(const base::BufferRef& input,
                                             const std::string& delimiter);
  bool resourceNotFound(const std::string& name, const std::string& value);
  bool badRequest(const char* msg = nullptr);
};
