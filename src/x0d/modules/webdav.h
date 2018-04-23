// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/Module.h>

namespace x0d {

class WebdavModule : public Module {
 public:
  explicit WebdavModule(Daemon* d);

  // main handler
  bool webdav(Context* cx, Params& args);

 private:
  bool webdav_mkcol(Context* cx);
  bool webdav_get(Context* cx);
  bool webdav_put(Context* cx, Params& args);
  bool todo(Context* cx);
};

} // namespace x0d

