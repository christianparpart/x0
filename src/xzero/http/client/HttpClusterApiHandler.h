// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <xzero/CustomDataMgr.h>
#include <xzero/Buffer.h>

namespace xzero {

namespace http {

class HttpRequest;
class HttpResponse;

namespace client {

class HttpClusterApi;
class HttpCluster;

class HttpClusterApiHandler : public CustomData {
 public:
  HttpClusterApiHandler(
      HttpClusterApi* api,
      HttpRequest* request,
      HttpResponse* response,
      const BufferRef& prefix);
  ~HttpClusterApiHandler();

  bool run();

 private:
  void processIndex();
  void index();

  void processCluster();
  void createCluster(const std::string& name);
  void showCluster(HttpCluster* cluster);
  void updateCluster(HttpCluster* cluster);
  void destroyCluster(HttpCluster* cluster);

  bool processBackend();
  bool createBackend(HttpCluster* cluster);
  void showBackend();
  void updateBackend();
  void lockBackend();
  void unlockBackend();
  void destroyBackend();

  bool processBucket();
  bool createBucket(HttpCluster* cluster);
  void showBucket();
  void updateBucket();
  void destroyBucket();

  bool badRequest(const char* msg);
  bool methodNotAllowed();

 private:
  HttpClusterApi* api_;
  HttpRequest* request_;
  HttpResponse* response_;
  BufferRef prefix_;
  std::vector<std::string> tokens_;
  std::unordered_map<std::string, std::string> params_;
};

} // namespace client
} // namespace http
} // namespace xzero
