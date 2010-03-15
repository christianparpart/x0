/* <x0/plugin.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_plugin_hpp
#define x0_plugin_hpp (1)

#include <x0/types.hpp>
#include <x0/api.hpp>

namespace x0 {

//! \addtogroup core
//@{

class server;
class context;

/**
 * \brief base class for all plugins for use within this x0 web server.
 *
 * \see server, connection, request, response
 */
class plugin
{
private:
	plugin(const plugin&) = delete;
	plugin& operator=(const plugin&) = delete;

public:
	/** initializes the plugin */
	plugin(server& srv, const std::string& name) :
		server_(srv), name_(name)
	{
	}

	/** finalizes the plugin */
	~plugin()
	{
	}

	/** retrieves the plugin's unique basename (index, userdir, sendfile, auth, etc...) */
	std::string name() const
	{
		return name_;
	}

	/** gets invoked at (re)configure time */
	virtual void configure()
	{
	}

	/** merges a configuration context.
	 * \param to the context destination to merge the \p from_data to.
	 * \param from_data the configuration from the source context.
	 * \see context, context::merge()
	 */
	virtual void merge(context& to, void *from_data)
	{
	}

protected:
	server& server_;
	std::string name_;
};

typedef std::shared_ptr<plugin> plugin_ptr;

#define X0_EXPORT_PLUGIN(pluginName) \
	X0_EXPORT_PLUGIN_CLASS(pluginName, pluginName##_plugin)

#define X0_EXPORT_PLUGIN_CLASS(pluginName, className) \
	extern "C" x0::plugin *pluginName##_init(x0::server& srv, const std::string& name) { \
		return new className(srv, name); \
	}

//@}

} // namespace x0

#endif
