/* <x0/plugins/BackendManager.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include "BackendManager.h"

BackendManager::BackendManager(x0::HttpWorker* worker, const std::string& name) :
#ifndef NDEBUG
	x0::Logging("BackendManager/%s", name.c_str()),
#endif
	worker_(worker),
	name_(name),
	connectTimeout_(x0::TimeSpan::fromSeconds(10)),
	readTimeout_(x0::TimeSpan::fromSeconds(120)),
	writeTimeout_(x0::TimeSpan::fromSeconds(10))
{
}

BackendManager::~BackendManager()
{
}

