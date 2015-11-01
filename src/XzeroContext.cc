// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "XzeroContext.h"
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpStatus.h>
#include <xzero/net/IPAddress.h>
#include <xzero/WallClock.h>
#include <xzero/UnixTime.h>
#include <xzero/logging.h>

using namespace xzero;
using namespace xzero::http;

namespace x0d {

XzeroContext::XzeroContext(
    xzero::flow::vm::Handler* entrypoint,
    xzero::http::HttpRequest* request,
    xzero::http::HttpResponse* response)
    : runner_(entrypoint->createRunner()),
      createdAt_(now()),
      request_(request),
      response_(response),
      documentRoot_(),
      pathInfo_(),
      file_(),
      errorHandler_(nullptr) {
  runner_->setUserData(this);
  response_->onResponseEnd([this] {
    // explicitely wipe customdata before we're actually deleting the context
    clearCustomData();
    delete this;
  });
}

xzero::UnixTime XzeroContext::now() const {
  return WallClock::now();
}

xzero::Duration XzeroContext::duration() const {
  return now() - createdAt();
}

const IPAddress& XzeroContext::remoteIP() const {
  if (request_->remoteIP().isSome())
    return request_->remoteIP().get();

  RAISE(RuntimeError, "Non-IP transport channels not supported");
}

int XzeroContext::remotePort() const {
  return -1; // TODO
}

size_t XzeroContext::bytesReceived() const {
  return request_->bytesReceived();
}

size_t XzeroContext::bytesTransmitted() const {
  return response_->bytesTransmitted();
}

bool XzeroContext::verifyDirectoryDepth() {
  if (request()->directoryDepth() < 0) {
    logError("x0d", "Directory traversal detected: $0", request()->path());
    // TODO: why not throwing BadMessage here (anymore?) ?
    //throw BadMessage(HttpStatus::BadRequest, "Directory traversal detected");
    response()->setStatus(HttpStatus::BadRequest);
    response()->setReason("Directory traversal detected");
    response()->completed();
    return false;
  }
  return true;
}

} // namespace x0d
