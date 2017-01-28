// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroModule.h>

namespace x0d {

class WebdavModule : public XzeroModule {
 public:
  explicit WebdavModule(XzeroDaemon* d);

  // main handler
  bool webdav(XzeroContext* cx, Params& args);

 private:
  bool webdav_mkcol(XzeroContext* cx);
  bool webdav_get(XzeroContext* cx);
  bool webdav_put(XzeroContext* cx, Params& args);
  bool todo(XzeroContext* cx);
};

} // namespace x0d

