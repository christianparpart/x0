// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <unordered_map>
#include <memory>
#include <cassert>

namespace xzero {

struct XZERO_BASE_API CustomData {
  CustomData(const CustomData&) = delete;
  CustomData& operator=(const CustomData&) = delete;
  CustomData() = default;
  virtual ~CustomData() {}
};

#define CUSTOMDATA_API_INLINE                                              \
 private:                                                                  \
  std::unordered_map<const void*, std::unique_ptr<xzero::CustomData>>      \
  customData_;                                                             \
 public:                                                                   \
  void clearCustomData() { customData_.clear(); }                          \
                                                                           \
  void clearCustomData(void* key) {                                        \
    auto i = customData_.find(key);                                        \
    if (i != customData_.end()) {                                          \
      customData_.erase(i);                                                \
    }                                                                      \
  }                                                                        \
                                                                           \
  xzero::CustomData* customData(const void* key) const {                   \
    auto i = customData_.find(key);                                        \
    return i != customData_.end() ? i->second.get() : nullptr;             \
  }                                                                        \
                                                                           \
  template <typename T>                                                    \
  T* customData(const void* key) const {                                   \
    auto i = customData_.find(key);                                        \
    return i != customData_.end() ? static_cast<T*>(i->second.get())       \
                                  : nullptr;                               \
  }                                                                        \
                                                                           \
  xzero::CustomData* setCustomData(const void* key,                            \
                                std::unique_ptr<xzero::CustomData>&& value) {  \
    auto res = value.get();                                                \
    customData_[key] = std::move(value);                                   \
    return res;                                                            \
  }                                                                        \
                                                                           \
  template <typename T, typename... Args>                                  \
  T* setCustomData(const void* key, Args&&... args) {                      \
    auto i = customData_.find(key);                                        \
    if (i != customData_.end()) {                                          \
      assert(dynamic_cast<T*>(i->second.get()) != nullptr);                \
      return static_cast<T*>(i->second.get());                             \
    }                                                                      \
                                                                           \
    T* value = new T(std::forward<Args>(args)...);                         \
    customData_[key].reset(value);                                         \
    return value;                                                          \
  }

}  // namespace xzero
