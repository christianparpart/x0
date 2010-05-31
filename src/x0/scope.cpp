#include <x0/scope.hpp>

namespace x0 {

void scope_value::merge(const scope_value *)
{
}

void scope::set(void *key, std::shared_ptr<scope_value> value)
{
	data_[key] = value;
}

void scope::merge(const scope_value *from)
{
	const scope *s = dynamic_cast<const scope *>(from);

	if (!s)
		return;

	// iterate through each value from source scope and "merge" it to this scope
	for (auto i = s->data_.begin(), e = s->data_.end(); i != e; ++i)
	{
		auto k = data_.find(i->first);

		// if this scope does also contain current key
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
