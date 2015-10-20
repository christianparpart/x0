// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "XzeroWorker.h"
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpStatus.h>
#include <xzero/StringUtil.h>
#include <xzero/executor/NativeScheduler.h>

namespace x0d {

using namespace xzero;
using namespace xzero::http;

XzeroWorker::XzeroWorker(XzeroDaemon* d, unsigned id, bool threaded)
    : daemon_(d),
      id_(id),
      name_(StringUtil::format("x0d/$0", id)),
      startupTime_(),
      now_(),
      scheduler_(new NativeScheduler()),
      thread_() {

  if (threaded) {
    thread_ = std::thread(std::bind(&XzeroWorker::runLoop, this));
  }
}

XzeroWorker::~XzeroWorker() {
}

void XzeroWorker::runLoop() {
  scheduler_->runLoop();
}

void XzeroWorker::handleRequest(HttpRequest* request, HttpResponse* response) {
  std::string host = request->headers().get("Host");

  std::string body = StringUtil::format("Proxy $0 http://$1$2\n", 
      request->unparsedMethod(), host, request->path());

  response->setStatus(HttpStatus::Ok);
  response->setReason("since because");
  response->setContentLength(body.size());
  response->output()->write(body);
  response->completed();
};

} // namespace x0d
