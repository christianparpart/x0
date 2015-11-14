#include "XzeroModule.h"

namespace x0d {

class WebdavModule : public XzeroModule {
 public:
  explicit WebdavModule(XzeroDaemon* d);

  // main handlers
  bool webdav_mkcol(XzeroContext* cx, Params& args);
  bool webdav_put(XzeroContext* cx, Params& args);
};

} // namespace x0d

