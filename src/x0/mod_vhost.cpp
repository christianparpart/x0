#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/config.hpp>
#include <x0/vhost_selector.hpp>
#include <x0/vhost.hpp>
#include <boost/lexical_cast.hpp>

namespace x0 {

class vhost_plugin :
	public plugin
{
private:
	std::map<vhost_selector, vhost_ptr> vhosts_;

public:
	vhost_plugin(server& srv) :
		plugin(srv)
	{
		// setup hooks
		server_.document_root_resolver.connect(bindMember(&vhost_plugin::document_root_resolver, this));

		// populate vhosts database
		config vhosts;
		vhosts.load_file(server_.get_config().get("service", "vhosts-file"));

		for (auto i = vhosts.cbegin(); i != vhosts.cend(); ++i)
		{
			std::string hostname = i->first;
			int port = std::atoi(vhosts.get(hostname, "port").c_str());

			vhosts_[vhost_selector(hostname, port)].reset(new vhost(i->second));
			std::cerr << "register vhost: " << hostname << " (port " << port << ")" << std::endl;

			server_.setup_listener(port);
		}
	}

	~vhost_plugin()
	{
		server_.document_root_resolver.disconnect(bindMember(&vhost_plugin::document_root_resolver, this));
	}

private:
	void document_root_resolver(request& r) {
		if (!r.document_root.empty())
			return;

		vhost_selector selector(r.get_header("Host"), r.connection->socket().local_endpoint().port());
		auto vhi = vhosts_.find(selector);

		if (vhi != vhosts_.end())
		{
			r.document_root = vhi->second->config_section["document_root"];
			// XXX maybe assign more vhost-related attributes to this request, e.g. AdminEMail, AdminName, etc.
		}
	}
};

void vhost_init(server& srv) {
	srv.setup_plugin(plugin_ptr(new vhost_plugin(srv)));
}

} // namespace x0
