/* <x0/mod_vhost_basic.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * \ingroup modules
 * \brief provides a basic config-file based virtual hosting facility.
 */
class vhost_basic_plugin :
	public x0::plugin
{
private:
	boost::signals::connection c;

	struct vhost_config
	{
		std::string docroot;
	};

public:
	vhost_basic_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		c = server_.resolve_document_root.connect(boost::bind(&vhost_basic_plugin::resolve_document_root, this, _1));
	}

	~vhost_basic_plugin()
	{
		server_.resolve_document_root.disconnect(c);
	}

	virtual void configure()
	{
		auto hostnames = server_.config()["Hosts"].keys<std::string>();

		for (auto i = hostnames.begin(), e = hostnames.end(); i != e; ++i)
		{
			std::string hostname(*i);
			vhost_config& cfg = server_.create_context<vhost_config>(this, hostname);
			cfg.docroot = server_.config()["Hosts"][hostname]["DocumentRoot"].as<std::string>();

			int port = extract_port(hostname);

			std::string bind_address = server_.config()["Hosts"][hostname]["BindAddress"].as<std::string>();
			if (bind_address.empty())
				bind_address = "0::0";

			server_.log(x0::severity::debug, "port=%d: hostname[%s]: docroot[%s]", port, hostname.c_str(), cfg.docroot.c_str());

			server_.setup_listener(port, bind_address);
		}
	}

	int extract_port(const std::string& hostname)
	{
		std::size_t n = hostname.find(":");
		if (n == std::string::npos)
			return 80; // default

		return boost::lexical_cast<int>(hostname.substr(n + 1));
	}

private:
	void resolve_document_root(x0::request& in)
	{
		try
		{
			std::string hostname(in.header("Host"));
			vhost_config& cfg = server_.context<vhost_config>(this, hostname);
			in.document_root = cfg.docroot;
		}
		catch (const x0::host_not_found&)
		{
			// eat up
		}
	}
};

X0_EXPORT_PLUGIN(vhost_basic);
