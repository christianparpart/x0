/* <x0/Url.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#if !defined(sw_x0_url_hpp)
#define sw_x0_url_hpp (1)

#include <x0/Api.h>
#include <x0/Buffer.h>
#include <string>
#include <unordered_map>

namespace x0 {

class X0_API Url
{
public:
	typedef std::unordered_map<std::string, std::string> ArgsMap;

	static Url parse(const std::string& url);

	Url();
	Url(const Url& url);
	Url(Url&& url);
	~Url();

	Url& operator=(const Url& url);

	const std::string& protocol() const { return protocol_; }
	const std::string& username() const { return username_; }
	const std::string& password() const { return password_; }
	const std::string& hostname() const { return hostname_; }
	int port() const { return port_; }
	const std::string& path() const { return path_; }
	const std::string& query() const { return query_; }
	const std::string& fragment() const { return fragment_; }

	// helper

	ArgsMap parseQuery() const { return parseQuery(query_.data(), query_.data() + query_.size()); }

	static ArgsMap parseQuery(const std::string& query) { return parseQuery(query.data(), query.data() + query.size()); }
	static std::string decode(const std::string& value) { return decode(value.data(), value.data() + value.size()); }

	static ArgsMap parseQuery(const BufferRef& query) { return parseQuery(query.data(), query.data() + query.size()); }
	static std::string decode(const BufferRef& value) { return decode(value.data(), value.data() + value.size()); }

	static ArgsMap parseQuery(const char* begin, const char* end);
	static std::string decode(const char* begin, const char* end);

private:
	std::string protocol_;
	std::string username_;
	std::string password_;
	std::string hostname_;
	int port_;
	std::string path_;
	std::string query_;
	std::string fragment_;
};

X0_API bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port, std::string& path, std::string& query);
X0_API bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port, std::string& path);
X0_API bool parseUrl(const std::string& spec, std::string& protocol, std::string& hostname, int& port);

// {{{ inline impl
inline std::string Url::decode(const char* begin, const char* end) {
	Buffer sb;

	for (const char* i = begin; i != end; ++i) {
		if (*i == '%') {
			char snum[3] = { *i, *++i, '\0' };
			sb.push_back((char)(strtol(snum, 0, 16) & 0xFF));
		} else if (*i == '+') {
			sb.push_back(' ');
		} else {
			sb.push_back(*i);
		}
	}
	return sb.str();
}

inline Url::ArgsMap Url::parseQuery(const char* begin, const char* end)
{
	ArgsMap args;

	for (const char *p = begin; p != end; ) {
		size_t len = 0;
		const char *q = p;

		while (*q && *q != '=' && *q != '&') {
			++q;
			++len;
		}

		if (len) {
			std::string name(p, 0, len);
			p += *q == '=' ? len + 1 : len;

			len = 0;
			for (q = p; *q && *q != '&'; ++q, ++len)
				;

			if (len) {
				std::string value(p, 0, len);
				p += len;

				for (; *p == '&'; ++p)
					; // consume '&' chars (usually just one)

				args[decode(name)] = decode(value);
			} else {
				if (*p) {
					++p;
				}

				args[decode(name)] = "";
			}
		} else if (*p) { // && or ?& or &=
			++p;
		}
	}

	return std::move(args);
}
// }}}

} // namespace x0

#endif
