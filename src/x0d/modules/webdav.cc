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

// TODO: PUT to actually store the file instead of echo'ing back

namespace x0d {

WebdavModule::WebdavModule(x0d::XzeroDaemon* d)
    : XzeroModule(d, "webdav") {

  mainHandler("webdav.put", &WebdavModule::put);
}

bool WebdavModule::put(XzeroContext* cx, Params& args) {
  if (!cx->verifyDirectoryDepth())
    return true;

  Buffer sstr;
  cx->request()->input()->read(&sstr);
  cx->response()->setStatus(HttpStatus::Ok);
  cx->response()->setContentLength(sstr.size());
  cx->response()->headers().overwrite("Content-Type",
      !cx->request()->headers().get("Content-Type").empty()
          ? cx->request()->headers().get("Content-Type")
          : daemon().mimetypes().defaultMimeType());
  cx->response()->output()->write(std::move(sstr));
  cx->response()->completed();

  return true;
}

} // namespace x0d
