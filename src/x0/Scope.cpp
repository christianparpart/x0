/* <x0/Scope.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/Scope.h>

namespace x0 {

void ScopeValue::merge(const ScopeValue *)
{
}

void Scope::set(const void *key, std::shared_ptr<ScopeValue> value)
{
	data_[key] = value;
}

void Scope::release(const void *key)
{
	auto i = data_.find(key);
	if (i != data_.end())
		data_.erase(i);
}

void Scope::merge(const ScopeValue *from)
{
	const Scope *s = dynamic_cast<const Scope *>(from);

	if (!s)
		return;

	// iterate through each value from source Scope and "merge" it to this Scope
	for (auto i = s->data_.begin(), e = s->data_.end(); i != e; ++i)
	{
		auto k = data_.find(i->first);

		// if this Scope does also contain current key
		if (k != data_.end())
		{
			// merge source value into destination value
			k->second->merge(i->second.get());
		}
		else
		{
			// ref source value
			data_[i->first] = i->second;
		}
	}
}

} // namespace x0
