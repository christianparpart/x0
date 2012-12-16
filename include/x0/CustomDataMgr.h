/* <x0/CustomDataMgr.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_CustomDataMgr_h
#define sw_x0_CustomDataMgr_h (1)

#include <x0/Types.h>
#include <unordered_map>
#include <memory>
#include <cassert>

namespace x0 {

class X0_API CustomData
{
private:
	CustomData(const CustomData&) = delete;
	CustomData& operator=(const CustomData&) = delete;

public:
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
	template<typename T, typename... Args>                               \
	T* setCustomData(const void *key, Args&&... args)                    \
	{                                                                    \
		auto i = customData_.find(key);                                  \
		if (i != customData_.end())                                      \
			return static_cast<T*>(i->second.get());                     \
                                                                         \
		T* value = new T(args...);                                       \
		customData_[key].reset(value);                                   \
		return value;                                                    \
	}

} // namespace x0

#endif
