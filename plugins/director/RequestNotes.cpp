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

using namespace x0;

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
		// XXX We should never reach here, because tokens must have been put back by Director::release() already.
		bucket->put(tokens);
		tokens = 0;
	}
}


void RequestNotes::setCacheKey(const char* i, const char* e)
{
	Buffer result;

	while (i != e) {
		if (*i == '%') {
			++i;
			if (i != e) {
				switch (*i) {
					case 's': // scheme
						if (!request->connection.isSecure())
							result.push_back("http");
						else
							result.push_back("https");
						break;
					case 'h': // host header
						result.push_back(request->requestHeader("Host"));
						break;
					case 'r': // request path
						result.push_back(request->path);
						break;
					case 'q': // query args
						result.push_back(request->query);
						break;
					case '%':
						result.push_back(*i);
						break;
					default:
						result.push_back('%');
						result.push_back(*i);
				}
				++i;
			} else {
				result.push_back('%');
				break;
			}
		} else {
			result.push_back(*i);
			++i;
		}
	}

	cacheKey = result.str();
	printf("cache key: '%s'\n", cacheKey.c_str());
}
