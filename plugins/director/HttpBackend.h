#pragma once

#include "Backend.h"

/*!
 * implements the HTTP backend.
 *
 * \see FastCgiBackend
 */
class HttpBackend : public Backend {
private:
	class ProxyConnection;

	std::string hostname_;
	int port_;

public:
	//HttpBackend(Director* director, const std::string& name, size_t capacity, const std::string& url);
	HttpBackend(Director* director, const std::string& name, size_t capacity, const std::string& hsotname, int port);
	~HttpBackend();

	virtual bool process(x0::HttpRequest* r);
	virtual size_t writeJSON(x0::Buffer& output) const;

	const std::string& hostname() const { return hostname_; }
	int port() const { return port_; }

private:
	ProxyConnection* acquireConnection();
};


