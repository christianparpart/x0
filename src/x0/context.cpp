#include <x0/context.hpp>
#include <x0/plugin.hpp>

namespace x0 {

/** merges another context into this context.
 *
 * \param from the source context to merge into this one.
 *
 * \see plugin::merge()
 */
void context::merge(context& from)
{
	for (map_type::iterator i = from.data_.begin(), e = from.data_.end(); i != e; ++i)
	{
		x0::plugin *plugin = i->first;
		void *data = i->second;

		if (data_.find(plugin) != data_.end())
		{
			; // TODO plugin->merge(*this, data);
		}
		else
		{
			set(plugin, data);
		}
	}
}

} // namespace x0
