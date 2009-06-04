/* <x0/mod_vhost.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/config.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * \ingroup modules
 * \brief provides a basic virtual hosting facility.
 */
class vhost_plugin :
	public x0::plugin
{
private:
	std::string server_root_;
	std::string default_host_;
	std::string document_root_;
	boost::signals::connection c;

public:
	vhost_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		c = server_.resolve_document_root.connect(boost::bind(&vhost_plugin::resolve_document_root, this, _1));
	}

	~vhost_plugin()
	{
		server_.resolve_document_root.disconnect(c);
	}

	virtual void configure()
	{
		server_root_ = server_.get_config().get("vhost", "server-root");		// e.g. /var/www/
		default_host_ = server_.get_config().get("vhost", "default-host");		// e.g. localhost
		document_root_ = server_.get_config().get("vhost", "document-root");	// e.g. /htdocs

		// enforce trailing slash at the end of server root
		if (!server_root_.empty() && server_root_[server_root_.length() - 1] != '/')
		{
			server_root_ += '/';
		}

		if (!document_root_.empty())
		{
			// enforce non-trailing-slash at the end of document root
			if (document_root_[document_root_.length() - 1] == '/')
			{
				document_root_ = document_root_.substr(0, document_root_.size() - 2);
			}

			// enforce leading slash at document root
			if (document_root_[0] != '/')
			{
				document_root_.insert(0, "/");
			}
		}
	}

private:
	void resolve_document_root(x0::request& in) {
		if (in.document_root.empty())
		{
			std::string host(in.get_header("Host"));

			std::string hostname(host);
			std::size_t n = hostname.find(":");
			if (n != std::string::npos)
			{
				hostname = hostname.substr(0, n);
			}

			std::string dr;
			dr.reserve(server_root_.size() + std::max(hostname.size(), default_host_.size()) + document_root_.size());
			dr += server_root_;
			dr += hostname;
			dr += document_root_;

			struct stat st;
			if (stat(dr.c_str(), &st) || !S_ISDIR(st.st_mode))
			{
				dr.clear();
				dr += server_root_;
				dr += default_host_;
				dr += document_root_;

				if (stat(dr.c_str(), &st) || !S_ISDIR(st.st_mode))
				{
					return;
				}
			}

			in.document_root = dr;
		}
	}
};

extern "C" x0::plugin *vhost_init(x0::server& srv, const std::string& name) {
	return new vhost_plugin(srv, name);
}
