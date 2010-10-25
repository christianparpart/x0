/* <x0/plugins/proxy/ProxyConnection.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_proxy_ProxyConnection_h
#define sw_x0_proxy_ProxyConnection_h (1)

#include <x0/WebClient.h>
#include <x0/Types.h>

#include <ev++.h>

class ProxyContext;
class ProxyConnection;

namespace x0 {
	class HttpRequest;
}

/** handles a connection from proxy to origin server.
 */
class ProxyConnection :
	public x0::WebClientBase
{
	friend class ProxyContext;

public:
	explicit ProxyConnection(ProxyContext *px);
	~ProxyConnection();

	void start(const std::function<void()>& done, x0::HttpRequest *r);

private:
	void connect(const std::string& origin);
	void disconnect();

	void pass_request();
	void pass_request_content(x0::BufferRef&& chunk);

	virtual void connect();
	virtual void response(int, int, int, x0::BufferRef&&);
	virtual void header(x0::BufferRef&&, x0::BufferRef&&);
	virtual bool content(x0::BufferRef&&);
	virtual bool complete();

	void content_written(int ec, std::size_t nb);

private:
	ProxyContext *px_;				//!< owning proxy

	std::string hostname_;			//!< origin's hostname
	int port_;						//!< origin's port
	std::function<void()> done_;
	x0::HttpRequest *request_;		//!< client's request
};

#endif
