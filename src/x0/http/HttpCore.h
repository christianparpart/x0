/* <HttpCore.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_HttpCore_h
#define sw_x0_HttpCore_h 1

#include <x0/http/HttpPlugin.h>

namespace x0 {

class HttpServer;
class SettingsValue;
class Scope;

class HttpCore :
	public HttpPlugin
{
public:
	explicit HttpCore(HttpServer& server);
	~HttpCore();

	Property<unsigned long long> max_fds;

	virtual bool post_config();

private:
	long long getrlimit(int resource);
	long long setrlimit(int resource, long long max);

	std::error_code setup_logging(const SettingsValue& cvar, Scope& s);
	std::error_code setup_resources(const SettingsValue& cvar, Scope& s);
	std::error_code setup_modules(const SettingsValue& cvar, Scope& s);
	std::error_code setup_fileinfo(const SettingsValue& cvar, Scope& s);
	std::error_code setup_error_documents(const SettingsValue& cvar, Scope& s);
	std::error_code setup_hosts(const SettingsValue& cvar, Scope& s);
	std::error_code setup_advertise(const SettingsValue& cvar, Scope& s);
};

} // namespace x0

#endif
