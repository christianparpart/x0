#include "XzeroModule.h"

namespace x0d {

class WebdavModule : public XzeroModule {
 public:
  explicit WebdavModule(XzeroDaemon* d);

  // main handlers
  bool put(XzeroContext* cx, Params& args);
};

} // namespace x0d

