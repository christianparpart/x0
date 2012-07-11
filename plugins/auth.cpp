/* <x0/plugins/auth.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 *
 * --------------------------------------------------------------------------
 *
 * plugin type: basic authentication
 *
 * description:
 *     Implements HTTP Basic Auth
 *
 * setup API:
 *     none
 *
 * request processing API:
 *     function auth.realm(string text);
 *     function auth.userfile(string path);
 *     handler auth.require();
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/http/HttpAuthUserFile.h>
#include <x0/Base64.h>
#include <x0/Types.h>

class AuthBasic : public x0::CustomData {
public:
	std::string realm;
	std::string userfile;

	AuthBasic() :
		realm("Restricted Area"),
		userfile()
	{}

	bool verify(const char* user, const char* pass) {
		x0::HttpAuthUserFile backend(userfile);
		return backend.authenticate(user, pass);
	};
};

class AuthPlugin :
	public x0::HttpPlugin
{
public:
	AuthPlugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerFunction<AuthPlugin, &AuthPlugin::auth_realm>("auth.realm");
		registerFunction<AuthPlugin, &AuthPlugin::auth_userfile>("auth.userfile");
		registerHandler<AuthPlugin, &AuthPlugin::auth_require>("auth.require");
	}

	~AuthPlugin()
	{
	}

private:
	void auth_realm(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result)
	{
		if (!r->customData<AuthBasic>(this))
			r->setCustomData<AuthBasic>(this);

		r->customData<AuthBasic>(this)->realm = args[0].toString();
	}

	void auth_userfile(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result)
	{
		if (!r->customData<AuthBasic>(this))
			r->setCustomData<AuthBasic>(this);

		r->customData<AuthBasic>(this)->userfile = args[0].toString();
	}

	bool auth_require(x0::HttpRequest *r, const x0::FlowParams& args)
	{
		AuthBasic* auth = r->customData<AuthBasic>(this);
		if (!auth) {
			r->log(x0::Severity::error, "auth.require used without specifying a backend");
			r->status = x0::HttpError::InternalServerError;
			r->finish();
			return true;
		}

		auto authorization = r->requestHeader("Authorization");
		if (!authorization)
			return sendAuthenticateRequest(r, auth->realm);

		if (authorization.begins("Basic ")) {
			x0::BufferRef authcode = authorization.ref(6);
			x0::Buffer plain = x0::Base64::decode(authcode);
			const char* user = plain.c_str();
			char* pass = strchr(const_cast<char*>(plain.c_str()), ':');
			if (!pass)
				return sendAuthenticateRequest(r, auth->realm);
			*pass++ = '\0';

			r->username = user;

			r->log(x0::Severity::debug, "auth.require: '%s' -> '%s'", authcode.str().c_str(), plain.c_str());

			if (auth->verify(user, pass)) {
				// authentification succeed, so do not intercept request processing
				return false;
			}
		}

		// authentification failed, one way or the other
		return sendAuthenticateRequest(r, auth->realm);
	}

	bool sendAuthenticateRequest(x0::HttpRequest* r, const std::string& realm)
	{
		char buf[1024];
		snprintf(buf, sizeof(buf), "Basic realm=\"%s\"", realm.c_str());
		r->responseHeaders.push_back("WWW-Authenticate", buf);
		r->status = x0::HttpError::Unauthorized;
		r->finish();
		return true;
	}
};

X0_EXPORT_PLUGIN_CLASS(AuthPlugin)
