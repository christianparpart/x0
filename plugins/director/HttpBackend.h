#pragma once

#include "Backend.h"

/*!
 * \brief implements an HTTP (reverse) proxy.
 *
 * \see FastCgiProxy
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

private:
	ProxyConnection* acquireConnection();
};


