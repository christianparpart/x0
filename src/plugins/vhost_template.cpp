/* <x0/plugins/vhost_template.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * \ingroup plugins
 * \brief provides a basic template-based mass virtual hosting facility.
 */
class vhost_template_plugin :
	public x0::HttpPlugin
{
private:
	std::string server_root_;
	std::string default_host_;
	std::string document_root_;
	x0::HttpServer::RequestHook::Connection c;

public:
	vhost_template_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		c = server_.onResolveDocumentRoot.connect<vhost_template_plugin, &vhost_template_plugin::resolveDocumentRoot>(this);
	}

	~vhost_template_plugin()
	{
		server_.onResolveDocumentRoot.disconnect(c);
	}

	virtual void configure()
	{
		// setup defaults
		server_root_ = "/var/www/";
		default_host_ = "localhost";
		document_root_ = "/htdocs";

		// load server context
		server_.config().load("HostTemplate.ServerRoot", server_root_); // e.g. /var/www/
		server_.config().load("HostTemplate.DefaultHost", default_host_); // e.g. localhost
		server_.config().load("HostTemplate.DocumentRoot", document_root_); // e.g. /htdocs

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

		// setup listener port and bind address
		int port(80);
		std::string bind("0::0");

		server_.config().load("HostTemplate.Listener", port);
		server_.config().load("HostTemplate.BindAddress", bind);

		server_.setupListener(port, bind);
	}

private:
	void resolveDocumentRoot(x0::HttpRequest *in) {
		if (in->document_root.empty())
		{
			x0::BufferRef hostname(in->header("Host"));

			std::size_t n = hostname.find(":");
			if (n != x0::BufferRef::npos)
			{
				hostname = hostname.ref(0, n);
			}

			std::string dr;
			dr.reserve(server_root_.size() + std::max(hostname.size(), default_host_.size()) + document_root_.size());
			dr += server_root_;
			dr += hostname.str();
			dr += document_root_;

			x0::FileInfoPtr fi = in->connection.server().fileinfo(dr);
			if (!fi || !fi->is_directory())
			{
				dr.clear();
				dr += server_root_;
				dr += default_host_;
				dr += document_root_;

				fi = in->connection.server().fileinfo(dr);
				if (!fi || !fi->is_directory())
				{
					return;
				}
			}

			in->document_root = dr;
		}
	}
};

X0_EXPORT_PLUGIN(vhost_template)
