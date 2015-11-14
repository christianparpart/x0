#include "XzeroModule.h"

namespace x0d {

class WebdavModule : public XzeroModule {
 public:
  explicit WebdavModule(XzeroDaemon* d);

  // main handler
  bool webdav(XzeroContext* cx, Params& args);

 private:
  bool webdav_mkcol(XzeroContext* cx);
  bool webdav_get(XzeroContext* cx);
  bool webdav_put(XzeroContext* cx);
  bool todo(XzeroContext* cx);
};

} // namespace x0d

