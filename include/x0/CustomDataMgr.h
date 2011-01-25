/* <x0/CustomDataMgr.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_CustomDataMgr_h
#define sw_x0_CustomDataMgr_h (1)

#include <x0/Types.h>
#include <unordered_map>

namespace x0 {

class CustomDataMgr
{
private:
	std::unordered_map<void *, CustomData *> map_;

public:
	CustomDataMgr()
	{
	}

	~CustomDataMgr()
	{
		for (auto i: map_)
			delete i.second;
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

	void setCustomData(void *key, CustomData *value)
	{
		auto i = map_.find(key);
		if (i != map_.end()) {
			delete i->second;
			map_.erase(i);
		}
		map_[key] = value;
	}

	template<typename T, typename... Args> void setCustomData(void *key, Args&&... args)
	{
		setCustomData(key, new T(args...));
	}
};

} // namespace x0

#endif
