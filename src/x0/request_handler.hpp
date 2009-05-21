/* <x0/request_handler.hpp>
 * 
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_http_request_handler_hpp
#define x0_http_request_handler_hpp (1)

#include <x0/types.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <string>

namespace x0 {

struct request;
struct response;

typedef function<void(request&, response&)> request_handler_fn;

#if 0
// XXX old
class request_handler :
	private noncopyable
{
public:
	explicit request_handler(const std::string& docroot);

	void handle_request(const request& req, response& rep);

private:
	std::string docroot_;

	static bool url_decode(const std::string& in, std::string& out);
};
#endif

} // namespace x0

#endif
