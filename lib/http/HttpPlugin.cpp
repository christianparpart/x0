/* <x0/HttpPlugin.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
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
	// ensure that it's only the base-name we store
	// (fixes some certain cases where we've a path prefix supplied.)
	size_t i = name_.rfind('/');
	if (i != std::string::npos)
		name_ = name_.substr(i + 1);
}

/** \brief safely destructs the plugin.
  */
HttpPlugin::~HttpPlugin()
{
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

} // namespace x0
