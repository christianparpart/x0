/* <x0/context.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */
#ifndef x0_context_hpp
#define x0_context_hpp (1)

#include <x0/plugin.hpp>
#include <x0/api.hpp>

namespace x0 {

/**
 * A context object holds custom plugin-information, such as configuration settings and runtime states.
 *
 * We maintain different kinds of configuration contexts.
 * <ol>
 *   <li>request context - request-local context</li>
 *   <li>directory context - context wrt a particular directory prefix in underlying filesystem storage</li>
 *   <li>virtual host context - context wrt a particular virtual host</li>
 *   <li>server context - stores all globally applicable configuration settings and states</li>
 * </ol>
 *
 * \see server::context, request::context
 * \see plugin::merge()
 */
struct context
{
private:
	typedef std::map<plugin *, void *> map_type;
	map_type data_;

public:
	template<class T>
	T& set(plugin *p, T *d)
	{
		data_[p] = d;
		return *d;
	}

	void set(plugin *p, void *d)
	{
		data_[p] = d;
	}

	template<typename T>
	T& get(plugin *p)
	{
		return *static_cast<T *>(data_[p]);
	}

	template<typename T>
	T *free(plugin *p)
	{
		void *d = data_[p];
		data_.erase(data_.find(p));
		return static_cast<T *>(d);
	}

	void *free(plugin *p)
	{
		return free<void>(p);
	}

public:
	/** merges another context into this context.
	 * \param from the source context to merge into this one.
	 * \see plugin::merge()
	 */
	void merge(context& from)
	{
		for (map_type::iterator i = from.data_.begin(), e = from.data_.end(); i != e; ++i)
		{
			x0::plugin *plugin = i->first;
			void *data = i->second;

			if (data_.find(plugin) != data_.end())
			{
				plugin->merge(*this, data);
			}
			else
			{
				set(plugin, data);
			}
		}
	}
};

} // namespace x0

#endif
