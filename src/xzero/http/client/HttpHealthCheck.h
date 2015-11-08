// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <list>
#include <vector>
#include <xzero/thread/Future.h>
#include <xzero/http/client/HttpClient.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/net/IPAddress.h>
#include <xzero/CompletionHandler.h>
#include <xzero/Duration.h>
#include <xzero/Uri.h>
#include <xzero/stdtypes.h>
#include <utility>
#include <istream>

namespace xzero {

class InputStream;

namespace http {
namespace client {

class HttpHealthCheck {
 public:
  HttpHealthCheck(
      Scheduler* scheduler,
      const Uri& url,
      Duration interval,
      const std::vector<HttpStatus>& successCodes = {HttpStatus::Ok});

  void setUrl(const Uri& url);
  const Uri& url() const { return url_; }

  void setInterval(const Duration& interval);
  Duration interval() const { return interval_; }

  void setSuccessCodes(const std::vector<HttpStatus>& codes);
  const std::vector<HttpStatus>& successCodes() const { return successCodes_; };

  bool isHealthy() const;

 private:
  Scheduler* scheduler_;

  Uri url_;
  Duration interval_;
  std::vector<HttpStatus> successCodes_;

  std::unique_ptr<HttpClient> client_;
};

} // namespace client
} // namespace http
} // namespace xzero
