/* <x0/vhost.hpp>
 * 
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_vhost_hpp
#define x0_vhost_hpp (1)

#include <x0/config.hpp>
#include <x0/types.hpp>
#include <boost/shared_ptr.hpp>

namespace x0 {

struct vhost
{
	config::section config_section;

	vhost() :
		config_section()
	{
	}

	vhost(const vhost& v) :
		config_section(v.config_section)
	{
	}

	vhost(const config::section& s) :
		config_section(s)
	{
	}
};

typedef shared_ptr<vhost> vhost_ptr;

} // namespace x0

#endif
