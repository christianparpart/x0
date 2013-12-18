/* <XzeroPlugin.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0d/XzeroPlugin.h>
#include <x0d/XzeroDaemon.h>
#include <x0/http/HttpRequest.h>

namespace x0d {

using namespace x0;

/** \brief initializes the plugin.
  *
  * \param srv reference to owning server object
  * \param name unique and descriptive plugin-name.
  */
XzeroPlugin::XzeroPlugin(XzeroDaemon* daemon, const std::string& name) :
	daemon_(daemon),
	server_(daemon->server()),
	name_(name)
#if !defined(XZERO_NDEBUG)
	, debugLevel_(9)
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
XzeroPlugin::~XzeroPlugin()
{
}

bool XzeroPlugin::post_config()
{
	return true;
}

/** post-check hook, gets invoked after <b>every</b> configuration hook has been proceed successfully.
 *
 * \retval true everything turned out well.
 * \retval false something went wrong and x0 should not startup.
 */
bool XzeroPlugin::post_check()
{
	return true;
}

/** hook, invoked on log-cycle event, especially <b>SIGUSR</b> signal.
 */
void XzeroPlugin::cycleLogs()
{
}

} // namespace x0d
