/* <HttpRequestHandler.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_HttpRequestHandler_h
#define sw_x0_HttpRequestHandler_h (1)

#include <x0/Api.h>
#include <vector>

namespace x0 {

class HttpPlugin;
class HttpRequest;
class HttpResponse;

class IHttpRequestHandler
{
public:
	virtual ~IHttpRequestHandler() {}

	virtual bool handleRequest(HttpRequest *request, HttpResponse *response) = 0;
};

class HttpRequestHandler
{
private:
	std::vector<IHttpRequestHandler *> list_;

public:
	HttpRequestHandler();

	void connect(IHttpRequestHandler *handler);
	void disconnect(IHttpRequestHandler *handler);

	bool operator()(HttpRequest *in, HttpResponse *out);
};

// {{{ HttpRequestHandler impl
inline HttpRequestHandler::HttpRequestHandler() :
	list_()
{
}

inline void HttpRequestHandler::connect(IHttpRequestHandler *object)
{
	list_.push_back(object);
}

inline void HttpRequestHandler::disconnect(IHttpRequestHandler *object)
{
	for (auto i = list_.begin(), e = list_.end(); i != e; ++i)
	{
		if (*i == object)
		{
			list_.erase(i);
			return;
		}
	}
}

inline bool HttpRequestHandler::operator()(HttpRequest *in, HttpResponse *out)
{
	for (auto i = list_.begin(), e = list_.end(); i != e; ++i)
		if ((*i)->handleRequest(in, out))
			return true;

	return false;
}
// }}}

} // namespace x0

#endif
