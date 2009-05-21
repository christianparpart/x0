/* <x0/mod_vhost.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/config.hpp>
#include <x0/vhost_selector.hpp>
#include <x0/vhost.hpp>
#include <boost/lexical_cast.hpp>

/**
 * \ingroup modules
 * \brief provides a basic virtual hosting facility.
 */
class vhost_plugin :
	public x0::plugin
{
private:
	std::map<x0::vhost_selector, x0::vhost_ptr> vhosts_;

public:
	vhost_plugin(x0::server& srv) :
		plugin(srv)
	{
		// setup hooks
		server_.resolve_document_root.connect(x0::bindMember(&vhost_plugin::resolve_document_root, this));

		// populate vhosts database
		x0::config vhosts;
		vhosts.load_file(server_.get_config().get("service", "vhosts-file"));

		for (auto i = vhosts.cbegin(); i != vhosts.cend(); ++i)
		{
			std::string hostname = i->first;
			int port = std::atoi(vhosts.get(hostname, "port").c_str());

			vhosts_[x0::vhost_selector(hostname, port)].reset(new x0::vhost(i->second));
			std::cerr << "register vhost: " << hostname << " (port " << port << ")" << std::endl;

			server_.setup_listener(port);
		}
	}

	~vhost_plugin()
	{
		server_.resolve_document_root.disconnect(x0::bindMember(&vhost_plugin::resolve_document_root, this));
	}

private:
	void resolve_document_root(x0::request& r) {
		if (r.document_root.empty())
		{
			x0::vhost_selector selector(r.get_header("Host"), r.connection->socket().local_endpoint().port());
			auto vhi = vhosts_.find(selector);

			if (vhi != vhosts_.end())
			{
				r.document_root = vhi->second->config_section["document_root"];
			}
		}
	}
};

extern "C" void vhost_init(x0::server& srv) {
	srv.setup_plugin(x0::plugin_ptr(new vhost_plugin(srv)));
}
