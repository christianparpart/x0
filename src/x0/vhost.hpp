#ifndef x0_vhost_hpp
#define x0_vhost_hpp (1)

#include <x0/config.hpp>
#include <boost/shared_ptr.hpp>

namespace x0 {

struct vhost
{
	config::section config_section;

	vhost(const config::section& s) :
		config_section(s)
	{
	}
};

typedef boost::shared_ptr<vhost> vhost_ptr;

} // namespace x0

#endif
