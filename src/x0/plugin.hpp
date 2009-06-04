/* <x0/plugin.hpp>
 * 
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_plugin_hpp
#define x0_plugin_hpp (1)

#include <x0/types.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

namespace x0 {

class server;

/**
 * \ingroup core
 * \brief base class for all plugins for use within this x0 web server.
 *
 * \see server, connection, request, response
 */
class plugin :
	public boost::noncopyable
{
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

protected:
	server& server_;
	std::string name_;
};

typedef boost::shared_ptr<plugin> plugin_ptr;

} // namespace x0

#endif
