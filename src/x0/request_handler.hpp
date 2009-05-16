#ifndef x0_http_request_handler_hpp
#define x0_http_request_handler_hpp (1)

#include <boost/noncopyable.hpp>
#include <string>

namespace x0 {

struct request;
struct reply;

class request_handler :
	private boost::noncopyable
{
public:
	explicit request_handler(const std::string& docroot);

	void handle_request(const request& req, reply& rep);

private:
	std::string docroot_;

	static bool url_decode(const std::string& in, std::string& out);
};

} // namespace x0

#endif
