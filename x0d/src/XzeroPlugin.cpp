// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroPlugin.h>
#include <x0d/XzeroDaemon.h>
#include <x0/http/HttpRequest.h>
#include <x0/DebugLogger.h>

namespace x0d {

using namespace x0;

#if !defined(XZERO_NDEBUG)
#define TRACE(n, msg...) XZERO_DEBUG("XzeroPlugin", (n), msg)
#else
#define TRACE(n, msg...) /*!*/ ((void)0)
#endif

/** \brief initializes the plugin.
  *
  * \param srv reference to owning server object
  * \param name unique and descriptive plugin-name.
  */
XzeroPlugin::XzeroPlugin(XzeroDaemon* daemon, const std::string& name)
    : daemon_(daemon),
      server_(daemon->server()),
      name_(name),
      cleanups_(),
      natives_()
#if !defined(XZERO_NDEBUG)
      ,
      debugLevel_(9)
#endif
{
  TRACE(1, "initializing %s", name_.c_str());
  // ensure that it's only the base-name we store
  // (fixes some certain cases where we've a path prefix supplied.)
  size_t i = name_.rfind('/');
  if (i != std::string::npos) name_ = name_.substr(i + 1);
}

/** \brief safely destructs the plugin.
  */
XzeroPlugin::~XzeroPlugin() {
  TRACE(1, "destructing %s", name_.c_str());

  for (auto cleanup : cleanups_) {
    cleanup();
  }

  for (auto native : natives_) {
    daemon_->unregisterNative(native->name());
  }
}

bool XzeroPlugin::post_config() { return true; }

/** post-check hook, gets invoked after <b>every</b> configuration hook has been
 *proceed successfully.
 *
 * \retval true everything turned out well.
 * \retval false something went wrong and x0 should not startup.
 */
bool XzeroPlugin::post_check() { return true; }

/** hook, invoked on log-cycle event, especially <b>SIGUSR</b> signal.
 */
void XzeroPlugin::cycleLogs() {}

}  // namespace x0d
