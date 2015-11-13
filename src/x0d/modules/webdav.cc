#include "webdav.h"
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpInputListener.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/File.h>
#include <xzero/logging.h>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::flow;

// TODO: fix send100Continue() to be instantly flushed to client.
// TODO: PUT to actually store the file instead of echo'ing back

namespace x0d {

WebdavModule::WebdavModule(x0d::XzeroDaemon* d)
    : XzeroModule(d, "webdav") {

  mainHandler("webdav.put", &WebdavModule::put);
}

class Put : public CustomData, public HttpInputListener { // {{{
 public:
  Put(XzeroContext* cx);
  ~Put();

  void onContentAvailable() override;
  void onAllDataRead() override;
  void onError(const std::string& errorMessage) override;

 private:
  XzeroContext* cx_;
};

Put::Put(XzeroContext* cx) : cx_(cx) {
  cx->request()->input()->setListener(this);
}

Put::~Put() {
}

void Put::onContentAvailable() {
}

void Put::onAllDataRead() {
  Buffer sstr;
  cx_->request()->input()->read(&sstr);
  cx_->response()->setStatus(HttpStatus::Ok);
  cx_->response()->setContentLength(sstr.size());
  cx_->response()->headers().overwrite("Content-Type", cx_->request()->headers().get("Content-Type"));
  cx_->response()->output()->write(std::move(sstr));
  cx_->response()->completed();
}

void Put::onError(const std::string& errorMessage) {
  // TODO
}
// }}}

bool WebdavModule::put(XzeroContext* cx, Params& args) {
  if (!cx->verifyDirectoryDepth())
    return true;

  cx->setCustomData<Put>(this, cx);

  return true;
}

} // namespace x0d
