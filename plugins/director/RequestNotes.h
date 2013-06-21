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

class Backend;

/** Additional request attributes when using the director cluster.
 *
 * \see Director
 */
class RequestNotes :
	public x0::CustomData
{
public:
	x0::DateTime ctime;
	Backend* backend;
	size_t tryCount;
#if defined(X0_DIRECTOR_CACHE)
	std::string cacheKey;
	x0::TimeSpan cacheTTL;
	std::list<std::string> cacheHeaderIgnores;
	bool cacheIgnore; //!< true if cache MUST NOT be preferred over the backend server's successful response.
#endif

	x0::TokenShaper<x0::HttpRequest>::Node* bucket; //!< the bucket (node) this request is to be scheduled via.
	size_t tokens; //!< contains the number of currently acquired tokens by this request (usually 0 or 1).

	explicit RequestNotes(x0::DateTime ct, Backend* backend = nullptr) :
		ctime(ct),
		backend(nullptr),
		tryCount(0),
#if defined(X0_DIRECTOR_CACHE)
		cacheKey(),
		cacheTTL(x0::TimeSpan::Zero),
		cacheHeaderIgnores(),
		cacheIgnore(false),
#endif
		bucket(nullptr),
		tokens(0)
	{}

	~RequestNotes()
	{
		if (bucket && tokens) {
			bucket->put(tokens);
		}

		//if (backend_) {
		//	backend_->director().scheduler().release(backend_);
		//}
	}
};
