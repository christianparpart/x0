#pragma once
/* <plugins/director/RequestNotes.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/CustomDataMgr.h>
#include <x0/DateTime.h>
#include <x0/TokenShaper.h>
#include <x0/TimeSpan.h>
#include <x0/sysconfig.h>
#include <cstring> // strlen()
#include <string>
#include <list>

namespace x0 {
	class HttpRequest;
}

class Backend;
class BackendManager;

/**
 * Additional request attributes when using the director cluster.
 *
 * @see Director
 */
struct RequestNotes :
	public x0::CustomData
{
	x0::HttpRequest* request;	//!< The actual HTTP request.
	x0::DateTime ctime;			//!< Request creation time.
	BackendManager* manager;	//!< Designated cluster to load balance this request.
	Backend* backend;			//!< Designated backend to serve this request.
	size_t tryCount;            //!< Number of request schedule attempts.

	x0::TokenShaper<RequestNotes>::Node* bucket; //!< the bucket (node) this request is to be scheduled via.
	size_t tokens; //!< contains the number of currently acquired tokens by this request (usually 0 or 1).

#if defined(X0_DIRECTOR_CACHE)
	std::string cacheKey;
	x0::TimeSpan cacheTTL;
	std::list<std::string> cacheHeaderIgnores;
	bool cacheIgnore; //!< true if cache MUST NOT be preferred over the backend server's successful response.
#endif

	explicit RequestNotes(x0::HttpRequest* r);
	~RequestNotes();

	void setCacheKey(const char* data, const char* eptr);
	void setCacheKey(const char* data) { setCacheKey(data, data + std::strlen(data)); }
	void setCacheKey(const std::string& fmt) { setCacheKey(fmt.data(), fmt.data() + fmt.size()); }
};
