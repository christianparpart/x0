#pragma once
/* <plugins/director/RequestNotes.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

//#include "ClassfulScheduler.h"
#include <x0/CustomDataMgr.h>
#include <x0/DateTime.h>

class Backend;

/** Additional request attributes when using the director cluster.
 *
 * \see Director
 */
struct RequestNotes :
	public x0::CustomData
{
	x0::DateTime ctime;
	Backend* backend;
	size_t tryCount;

	std::string bucketName;
//	ClassfulScheduler::Bucket* bucket;

	explicit RequestNotes(x0::DateTime ct, Backend* b = nullptr) :
		ctime(ct),
		backend(b),
		tryCount(0),
		bucketName()//,
//		bucket(nullptr)
	{}

	~RequestNotes()
	{
#if 0 // temporarily disabled
		if (bucket) {
			bucket->put();
		}
#endif

		//if (backend_) {
		//	backend_->director().scheduler().release(backend_);
		//}
	}
};
