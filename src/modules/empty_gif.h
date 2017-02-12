// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/XzeroModule.h>

namespace x0d {

class EmptyGifModule : public XzeroModule {
 public:
  explicit EmptyGifModule(XzeroDaemon* d);

 private:
  bool empty_gif(XzeroContext* cx, Params& args);
};

} // namespace x0d
