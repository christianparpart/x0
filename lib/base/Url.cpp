/* <x0/Url.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/Url.h>

namespace x0 {

// {{{
/** Parses origin server URL.
 *
 * \param spec The URL to parse.
 * \param protocol The parsed protocol value.
 * \param hostname The parsed hostname value (only the hostname).
 * \param port The parsed port number.
 * \param path The parsed path part (may be empty).
 * \param query The parsed query part (may be empty).
 */
bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port, std::string& path, std::string& query)
{
	Url url = Url::parse(spec);
	if (url.protocol().empty())
		return false;

	protocol = url.protocol();
	hostname = url.hostname();
	port = url.port();
	path = url.path();
	query = url.query();
	return true;
}

bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port, std::string& path)
{
	std::string query;
	bool rv = parseUrl(spec, protocol, hostname, port, path, query);
	return rv && query.empty();
}

bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port)
{
	std::string path;
	std::string query;

	bool rv = parseUrl(spec, protocol, hostname, port, path, query);
	return rv && query.empty() && (path.empty() || path == "/");
}
// }}}

Url::Url() :
	protocol_(),
	username_(),
	password_(),
	hostname_(),
	port_(),
	path_(),
	query_(),
	fragment_()
{
}

Url::Url(const Url& url) :
	protocol_(url.protocol_),
	username_(url.username_),
	password_(url.password_),
	hostname_(url.hostname_),
	port_(url.port_),
	path_(url.path_),
	query_(url.query_),
	fragment_(url.fragment_)
{
}

Url::Url(Url&& url) :
	protocol_(std::move(url.protocol_)),
	username_(std::move(url.username_)),
	password_(std::move(url.password_)),
	hostname_(std::move(url.hostname_)),
	port_(std::move(url.port_)),
	path_(std::move(url.path_)),
	query_(std::move(url.query_)),
	fragment_(std::move(url.fragment_))
{
}

Url::~Url()
{
}

Url& Url::operator=(const Url& url)
{
	protocol_ = url.protocol_;
	username_ = url.username_;
	password_ = url.password_;
	hostname_ = url.hostname_;
	port_ = url.port_;
	path_ = url.path_;
	query_ = url.query_;
	fragment_ = url.fragment_;

	return *this;
}

Url Url::parse(const std::string& text)
{
	Url url;

	std::size_t i = text.find("://");
	if (i == std::string::npos)
		return url;

	url.protocol_ = text.substr(0, i);

	std::size_t k = text.find("/", i + 3);

	if (k != std::string::npos) {
		url.hostname_ = text.substr(i + 3, k - i - 3);
		url.path_ = text.substr(k);

		i = url.path_.find("?");
		if (i != std::string::npos) {
			url.query_ = url.path_.substr(i + 1);
			url.path_.resize(i);
		}
	} else {
		url.hostname_ = text.substr(i + 3);
	}

	i = url.hostname_.find_last_of(":");
	if (i != std::string::npos) {
		url.port_ = std::atoi(url.hostname_.c_str() + i + 1);
		url.hostname_.resize(i);
	}

	if (!url.port_) {
		if (url.protocol_ == "http")
			url.port_ = 80;
		else if (url.protocol_ == "https")
			url.port_ = 443;
	}

	return url;
}

} // namespace x0
