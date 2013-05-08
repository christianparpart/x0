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

	x0::TokenShaper<x0::HttpRequest>::Node* bucket; //!< the bucket (node) this request is to be scheduled via.
	size_t tokens; //!< contains the number of currently acquired tokens by this request (usually 0 or 1).

	explicit RequestNotes(x0::DateTime ct, Backend* backend = nullptr) :
		ctime(ct),
		backend(nullptr),
		tryCount(0),
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
