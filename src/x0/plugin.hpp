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
	explicit plugin(server& srv) :
		server_(srv)
	{
	}

	~plugin()
	{
	}

	virtual void configure()
	{
	}

protected:
	server& server_;
};

typedef boost::shared_ptr<plugin> plugin_ptr;

} // namespace x0

#endif
