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

public:
	HttpBackend(Director* director, const std::string& name, const x0::SocketSpec& socketSpec, size_t capacity);
	~HttpBackend();

	virtual const std::string& protocol() const;
	virtual bool process(x0::HttpRequest* r);

	const x0::SocketSpec& socketSpec() const { return socketSpec_; }

private:
	ProxyConnection* acquireConnection();
};


