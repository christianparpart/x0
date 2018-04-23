// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/CustomDataMgr.h>
#include <xzero/Buffer.h>
#include <unordered_map>
#include <string>
#include <vector>

namespace xzero {
  class Duration;
  class IPAddress;
}

namespace xzero::http {
  class HttpRequest;
  class HttpResponse;
  enum class HttpStatus;
}

namespace xzero::http::cluster {

class Api;
class Backend;
class Cluster;

class ApiHandler : public CustomData {
 public:
  ApiHandler(Api* api,
             HttpRequest* request,
             HttpResponse* response,
             const std::string& prefix);
  ~ApiHandler();

  bool run();

 private:
  using HttpStatus = xzero::http::HttpStatus;

  void processIndex();
  void index();

  void createBackendOrBucket();
  void processCluster();
  void createCluster(const std::string& name);
  void showCluster(Cluster* cluster);
  void updateCluster(Cluster* cluster);
  HttpStatus doUpdateCluster(Cluster* cluster, HttpStatus status);
  void enableCluster(Cluster* cluster);
  void disableCluster(Cluster* cluster);
  void destroyCluster(Cluster* cluster);

  void processBackend();
  void createBackend(Cluster* cluster, const std::string& name);
  void showBackend(Cluster* cluster, Backend* member);
  void updateBackend(Cluster* cluster, Backend* member);
  void enableBackend(Cluster* cluster, Backend* member);
  void disableBackend(Cluster* cluster, Backend* member);
  void destroyBackend(Cluster* cluster, Backend* member);

  void processBucket();
  void createBucket(Cluster* cluster, const std::string& name);
  void showBucket(Cluster* cluster, const std::string& name);
  void updateBucket(Cluster* cluster, const std::string& name);
  void destroyBucket(Cluster* cluster, const std::string& name);

  template<typename... Args>
  bool generateResponse(HttpStatus status, const std::string& msg, Args... args);
  bool generateResponse(HttpStatus status);

  bool hasParam(const std::string& key) const;
  bool loadParam(const std::string& key, bool* result);
  bool loadParam(const std::string& key, int* result);
  bool loadParam(const std::string& key, size_t* result);
  bool loadParam(const std::string& key, float* result);
  bool loadParam(const std::string& key, Duration* result);
  bool loadParam(const std::string& key, std::string* result);
  bool loadParam(const std::string& key, IPAddress* result);

  template<typename T>
  bool tryLoadParamIfExists(const std::string& key, T* result) {
    if (!hasParam(key))
      return true;

    return loadParam(key, result);
  }

 private:
  Api* api_;
  HttpRequest* request_;
  HttpResponse* response_;
  unsigned errorCount_;
  std::string prefix_;
  std::vector<std::string> tokens_;
  std::unordered_map<std::string, std::string> params_;
};

} // namespace xzero::http::cluster
