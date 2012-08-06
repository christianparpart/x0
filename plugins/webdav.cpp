/* <x0/plugins/webdav.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2011 Christian Parpart <trapni@gentoo.org>
 */

/*
 * MISSION:
 *
 * implement a WebDAV protocol to be used as NFS-competive replacement,
 * supporting efficient networked I/O, including partial PUTs (Content-Range)
 *
 * Should be usable as an NFS-replacement at our company at least.
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/io/BufferSource.h>
#include <x0/strutils.h>
#include <x0/Types.h>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

#include <cstring>
#include <cerrno>
#include <cstddef>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#define TRACE(msg...) DEBUG("hello: " msg)

namespace WebDAV { // {{{

/*! implements WebDAV's HTTP PUT method.
 *
 * Uploads can be tested e.g. via:
 *   curl -kv -X PUT --data-binary "@/etc/passwd" http://localhost:8081/test.txt
 */
class Put {
private:
	x0::HttpRequest* request_;
	int fd_;
	bool created_;

public:
	Put(x0::HttpRequest* r) :
		request_(r),
		fd_(-1),
		created_(false)
	{
	}

	bool execute()
	{
		TRACE("Put.file: %s\n", request_->fileinfo->path().c_str());

		if (request_->contentAvailable()) {
			created_ = !request_->fileinfo->exists();

			if (!created_)
				::unlink(request_->fileinfo->path().c_str());

			fd_ = ::open(request_->fileinfo->path().c_str(), O_WRONLY | O_CREAT, 0666);
			if (fd_ < 0) {
				perror("WebDav.Put(open)");
				request_->status = x0::HttpStatus::Forbidden;
				request_->finish();
				delete this;
				return true;
			}

			request_->setBodyCallback<Put, &Put::onContent>(this);
		} else {
			request_->status = x0::HttpStatus::NotImplemented;
			request_->finish();
			delete this;
		}
		return true;
	}

	void onContent(const x0::BufferRef& chunk)
	{
		if (chunk.empty()) {
			if (created_)
				request_->status = x0::HttpStatus::Created;
			else
				request_->status = x0::HttpStatus::NoContent;

			request_->finish();
			::close(fd_);

			delete this;
		} else {
			::write(fd_, chunk.data(), chunk.size());
			// check return code and possibly early abort request with error code indicating diagnostics
		}
	}
};

} // }}}

/**
 * \ingroup plugins
 * \brief example content generator plugin
 */
class WebDAVPlugin :
	public x0::HttpPlugin
{
public:
	WebDAVPlugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerHandler<WebDAVPlugin, &WebDAVPlugin::handleRequest>("webdav");
	}

	~WebDAVPlugin()
	{
	}

private:
	virtual bool handleRequest(x0::HttpRequest *r, const x0::FlowParams& args)
	{
		if (r->method == "GET") {
			return todo(r);
		} else if (r->method == "PUT") {
			return (new WebDAV::Put(r))->execute();
		} else if (r->method == "MKCOL") {
			return todo(r);
		} else if (r->method == "DELETE") {
			return todo(r);
		} else {
			r->status = x0::HttpStatus::MethodNotAllowed;
			r->finish();
			return true;
		}
	}

	bool todo(x0::HttpRequest* r) {
		r->status = x0::HttpStatus::NotImplemented;
		r->finish();
		return true;
	}
};

X0_EXPORT_PLUGIN_CLASS(WebDAVPlugin)
