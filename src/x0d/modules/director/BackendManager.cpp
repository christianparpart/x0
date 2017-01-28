// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "BackendManager.h"
#include <base/JsonWriter.h>

using namespace xzero;
using namespace base;

BackendManager::BackendManager(HttpWorker* worker, const std::string& name)
    :
#ifndef NDEBUG
      Logging("BackendManager/%s", name.c_str()),
#endif
      worker_(worker),
      name_(name),
      connectTimeout_(10_seconds),
      readTimeout_(120_seconds),
      writeTimeout_(10_seconds),
      clientAbortAction_(ClientAbortAction::Close),
      load_() {
}

BackendManager::~BackendManager() {}

void BackendManager::log(LogMessage&& msg) {
  msg.addTag(name_);
  worker_->log(std::move(msg));
}
