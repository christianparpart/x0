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

class ProxyConnection;

namespace x0 {
	class HttpRequest;
}

/** handles a connection from proxy to origin server.
 */
class ProxyConnection :
	public x0::WebClientBase
{
public:
	explicit ProxyConnection(struct ev_loop *loop);
	~ProxyConnection();

	void connect(const std::string& origin);
	void start(const std::function<void()>& done, x0::HttpRequest *r);

private:
	void disconnect();

	void passRequest();

	void onInputReady(/*...*/);
	void passRequestContent(x0::BufferRef&& chunk);

	virtual void onConnect();
	virtual void onResponse(int vmajor, int vminor, int code, x0::BufferRef&& message);
	virtual void onHeader(x0::BufferRef&& name, x0::BufferRef&& value);
	virtual bool onContentChunk(x0::BufferRef&& chunk);
	virtual bool onComplete();

	void onContentWritten(int ec, std::size_t nb);

private:
	std::string hostname_;			//!< origin's hostname
	int port_;						//!< origin's port
	std::function<void()> done_;	//!< request completion handler
	x0::HttpRequest *request_;		//!< client's request
};

#endif
