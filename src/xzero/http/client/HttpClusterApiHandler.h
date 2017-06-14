// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <xzero/CustomDataMgr.h>
#include <xzero/Buffer.h>

namespace xzero {

class Duration;
class IPAddress;

namespace http {

class HttpRequest;
class HttpResponse;
enum class HttpStatus;

namespace client {

class HttpClusterApi;
class HttpClusterMember;
class HttpCluster;

class HttpClusterApiHandler : public CustomData {
 public:
  HttpClusterApiHandler(
      HttpClusterApi* api,
      HttpRequest* request,
      HttpResponse* response,
      const std::string& prefix);
  ~HttpClusterApiHandler();

  bool run();

 private:
  using HttpStatus = xzero::http::HttpStatus;

  void processIndex();
  void index();

  void createBackendOrBucket();
  void processCluster();
  void createCluster(const std::string& name);
  void showCluster(HttpCluster* cluster);
  void updateCluster(HttpCluster* cluster);
  HttpStatus doUpdateCluster(HttpCluster* cluster, HttpStatus status);
  void enableCluster(HttpCluster* cluster);
  void disableCluster(HttpCluster* cluster);
  void destroyCluster(HttpCluster* cluster);

  void processBackend();
  void createBackend(HttpCluster* cluster, const std::string& name);
  void showBackend(HttpCluster* cluster, HttpClusterMember* member);
  void updateBackend(HttpCluster* cluster, HttpClusterMember* member);
  void enableBackend(HttpCluster* cluster, HttpClusterMember* member);
  void disableBackend(HttpCluster* cluster, HttpClusterMember* member);
  void destroyBackend(HttpCluster* cluster, HttpClusterMember* member);

  void processBucket();
  void createBucket(HttpCluster* cluster, const std::string& name);
  void showBucket(HttpCluster* cluster, const std::string& name);
  void updateBucket(HttpCluster* cluster, const std::string& name);
  void destroyBucket(HttpCluster* cluster, const std::string& name);

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
  HttpClusterApi* api_;
  HttpRequest* request_;
  HttpResponse* response_;
  unsigned errorCount_;
  std::string prefix_;
  std::vector<std::string> tokens_;
  std::unordered_map<std::string, std::string> params_;
};

} // namespace client
} // namespace http
} // namespace xzero
