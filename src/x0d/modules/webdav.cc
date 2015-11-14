#include "webdav.h"
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpInputListener.h>
#include <xzero/io/OutputStream.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/File.h>
#include <xzero/logging.h>

/*
 * module webdav
 * =============
 *
 * RFC: 4918
 */

using namespace xzero;
using namespace xzero::http;
using namespace xzero::flow;

// TODO: PUT to actually store the file instead of echo'ing back

namespace x0d {

WebdavModule::WebdavModule(x0d::XzeroDaemon* d)
    : XzeroModule(d, "webdav") {

  // mainHandler("webdav.propfind", &WebdavModule::webdav_propfind);   // 9.1
  // mainHandler("webdav.proppatch", &WebdavModule::webdav_proppatch); // 9.2
  mainHandler("webdav.mkcol", &WebdavModule::webdav_mkcol);         // 9.3
  // mainHandler("webdav.get", &WebdavModule::webdav_get);             // 9.4
  // mainHandler("webdav.post", &WebdavModule::webdav_post);           // 9.5
  // mainHandler("webdav.delete", &WebdavModule::webdav_delete);       // 9.6
  mainHandler("webdav.put", &WebdavModule::webdav_put);             // 9.7
  // mainHandler("webdav.copy", &WebdavModule::webdav_copy);           // 9.8
  // mainHandler("webdav.move", &WebdavModule::webdav_move);           // 9.9
  // mainHandler("webdav.lock", &WebdavModule::webdav_lock);           // 9.10
  // mainHandler("webdav.unlock", &WebdavModule::webdav_unlock);       // 9.11
}

bool WebdavModule::webdav_mkcol(XzeroContext* cx, Params& args) {
  if (!cx->verifyDirectoryDepth())
    return true;

  if (!cx->file()) {
    return false;
  }

  if (cx->file()->isDirectory()) {
    cx->response()->setStatus(HttpStatus::Ok);
    cx->response()->completed();
    return true;
  }

  logDebug("webdav", "Creating directory: $0", cx->file()->path());
  try {
    FileUtil::mkdir_p(cx->file()->path());
    cx->response()->setStatus(HttpStatus::Created);
  } catch (const std::exception& e) {
    logError("webdav", "Failed creating file $0. $1",
             cx->file()->path(),
             e.what());
    cx->response()->setStatus(HttpStatus::NoContent);
  }
  cx->response()->completed();

  return true;
}

bool WebdavModule::webdav_put(XzeroContext* cx, Params& args) {
  // TODO: pre-allocate full storage in advance
  // TODO: attempt native file rename/move into target location if possible

  if (!cx->verifyDirectoryDepth())
    return true;

  if (!cx->file())
    return false;

  // 9.7.2. PUT for Collections
  if (cx->file()->isDirectory()) {
    cx->response()->setStatus(HttpStatus::MethodNotAllowed);
    cx->response()->completed();
    return true;
  }

  Buffer content;
  cx->request()->input()->read(&content);

  logDebug("webdav", "put filename: $0", cx->file()->path());

  //bool didNotExistBefore = !cx->file()->exists();
  std::unique_ptr<OutputStream> output = cx->file()->createOutputChannel();

  // if (!output->tryAllocate(content.size())) {
  //   if (didNotExistBefore)
  //     FileUtil::rm(cx->file()->path());
  //   cx->response()->setStatus(HttpStatus::InsufficientStorage);
  //   cx->response()->completed();
  // }

  output->write(content.data(), content.size());

  cx->response()->setStatus(HttpStatus::Created);
  cx->response()->completed();

  return true;
}

} // namespace x0d
