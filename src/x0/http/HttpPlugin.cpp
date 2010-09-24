/* <x0/HttpPlugin.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpRequest.h>

namespace x0 {

/** \brief initializes the plugin.
  *
  * \param srv reference to owning server object
  * \param name unique and descriptive plugin-name.
  */
HttpPlugin::HttpPlugin(HttpServer& srv, const std::string& name) :
	server_(srv),
	name_(name)
#if !defined(NDEBUG)
	, debug_level_(9)
#endif
{
}

/** \brief safely destructs the plugin.
  */
HttpPlugin::~HttpPlugin()
{
	while (!cvars_.empty())
		undeclareCVar(cvars_[0]);

	// clean up possible traces in server and vhost scopes
	auto hostnames = server_.hostnames();
	for (auto i = hostnames.begin(), e = hostnames.end(); i != e; ++i)
		if (Scope *s = server_.resolveHost(*i))
			s->release(this);

	server_.release(this);
}

/** \brief retrieves the number of configuration variables registered by this plugin.
  */
const std::vector<std::string>& HttpPlugin::cvars() const
{
	return cvars_;
}

/** \brief unregisters given configuration variable.
  */
void HttpPlugin::undeclareCVar(const std::string& key)
{
	for (auto i = cvars_.begin(), e = cvars_.end(); i != e; ++i)
	{
		if (*i == key)
		{
			server_.undeclareCVar(key);
			cvars_.erase(i);
			return;
		}
	}
}

bool HttpPlugin::post_config()
{
	return true;
}

/** post-check hook, gets invoked after <b>every</b> configuration hook has been proceed successfully.
 *
 * \retval true everything turned out well.
 * \retval false something went wrong and x0 should not startup.
 */
bool HttpPlugin::post_check()
{
	return true;
}

/** request handler
 *
 * \retval true this plugin is taking over the request, handling it.
 * \retval false we do not want this request..
 */
bool HttpPlugin::handleRequest(HttpRequest *request, HttpResponse *response, const Params& params)
{
	return false;
}

void HttpPlugin::process(void *p, int argc, Flow::Value *argv)
{
	HttpPlugin *self = (HttpPlugin *)p;

	HttpRequest *in = self->server_.in_;
	HttpResponse *out = self->server_.out_;
	Params params(argc, argv + 1);

	argv[0] = self->handleRequest(in, out, params);
}

} // namespace x0
