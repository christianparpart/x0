/* <plugins/director/RequestNotes.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */
#include "RequestNotes.h"
#include "Backend.h"

#include <x0/http/HttpRequest.h>
#include <x0/http/HttpConnection.h>
#include <x0/http/HttpWorker.h>
#include <x0/CustomDataMgr.h>
#include <x0/DateTime.h>
#include <x0/TokenShaper.h>
#include <x0/TimeSpan.h>
#include <x0/sysconfig.h>

RequestNotes::RequestNotes(x0::HttpRequest* r) :
	request(r),
	ctime(r->connection.worker().now()),
	manager(nullptr),
	backend(nullptr),
	tryCount(0),
	bucket(nullptr),
	tokens(0)
#if defined(X0_DIRECTOR_CACHE)
	,
	cacheKey(),
	cacheTTL(x0::TimeSpan::Zero),
	cacheHeaderIgnores(),
	cacheIgnore(false)
#endif
{
}

RequestNotes::~RequestNotes()
{
	if (bucket && tokens) {
		bucket->put(tokens);
	}

	if (backend) {
		// Notify director that this backend has just completed a request,
		backend->release(this);
	}
}
