/* <x0/CustomDataMgr.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_CustomDataMgr_h
#define sw_x0_CustomDataMgr_h (1)

#include <x0/Types.h>
#include <unordered_map>
#include <memory>
#include <cassert>

namespace x0 {

struct X0_API CustomData
{
    CustomData(const CustomData&) = delete;
    CustomData& operator=(const CustomData&) = delete;
    CustomData() = default;
    virtual ~CustomData() {}
};

#define CUSTOMDATA_API_INLINE                                            \
private:                                                                 \
    std::unordered_map<const void *, std::unique_ptr<x0::CustomData>> customData_; \
public:                                                                  \
    void clearCustomData()                                               \
    {                                                                    \
        customData_.clear();                                             \
    }                                                                    \
                                                                         \
    void clearCustomData(void *key)                                      \
    {                                                                    \
        auto i = customData_.find(key);                                  \
        if (i != customData_.end()) {                                    \
            customData_.erase(i);                                        \
        }                                                                \
    }                                                                    \
                                                                         \
    x0::CustomData* customData(const void* key) const                    \
    {                                                                    \
        auto i = customData_.find(key);                                  \
        return i != customData_.end() ? i->second.get() : nullptr;       \
    }                                                                    \
                                                                         \
    template<typename T>                                                 \
    T* customData(const void* key) const                                 \
    {                                                                    \
        auto i = customData_.find(key);                                  \
        return i != customData_.end()                                    \
            ? static_cast<T*>(i->second.get())                           \
            : nullptr;                                                   \
    }                                                                    \
                                                                         \
    x0::CustomData* setCustomData(const void* key,                       \
                              std::unique_ptr<x0::CustomData>&& value)   \
    {                                                                    \
        auto res = value.get();                                          \
        customData_[key] = std::move(value);                             \
        return res;                                                      \
    }                                                                    \
                                                                         \
    template<typename T, typename... Args>                               \
    T* setCustomData(const void *key, Args&&... args)                    \
    {                                                                    \
        auto i = customData_.find(key);                                  \
        if (i != customData_.end())                                      \
            return static_cast<T*>(i->second.get());                     \
                                                                         \
        T* value = new T(std::forward<Args>(args)...);                   \
        customData_[key].reset(value);                                   \
        return value;                                                    \
    }

} // namespace x0

#endif
