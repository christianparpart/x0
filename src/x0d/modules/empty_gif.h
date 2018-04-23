// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/Module.h>

namespace x0d {

class EmptyGifModule : public Module {
 public:
  explicit EmptyGifModule(Daemon* d);

 private:
  bool empty_gif(Context* cx, Params& args);
};

} // namespace x0d
