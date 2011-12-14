/* <x0/CustomDataMgr.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_CustomDataMgr_h
#define sw_x0_CustomDataMgr_h (1)

#include <x0/Types.h>
#include <unordered_map>
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

class X0_API CustomDataMgr
{
private:
	std::unordered_map<void *, CustomData *> map_;

public:
	CustomDataMgr()
	{
	}

	~CustomDataMgr()
	{
#if 0
		assert(map_.empty() && "You must have invoked clearCustomData() in your parent destructor already to avoid unnecessary  bugs.");
#else
		if (!map_.empty())
			fprintf(stderr, "BUG: You must have invoked clearCustomData() in your parent destructor already to avoid unnecessary  bugs.");
#endif

		clearCustomData();
	}

	void clearCustomData()
	{
		for (auto i: map_)
			delete i.second;

		map_.clear();
	}

	CustomData *customData(void *key) const
	{
		auto i = map_.find(key);
		return i != map_.end() ? i->second : nullptr;
	}

	template<typename T>
	T *customData(void *key) const
	{
		auto i = map_.find(key);
		return i != map_.end() ? static_cast<T *>(i->second) : nullptr;
	}

	template<typename T, typename... Args> bool setCustomData(void *key, Args&&... args)
	{
		auto i = map_.find(key);
		if (i != map_.end())
			return false;

		map_[key] = new T(args...);
		return true;
	}
};

} // namespace x0

#endif
