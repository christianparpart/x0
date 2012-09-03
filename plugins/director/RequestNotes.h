/* <plugins/director/RequestNotes.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */
#pragma once

#include "ClassfulScheduler.h"
#include <x0/CustomDataMgr.h>
#include <x0/DateTime.h>

class Backend;

struct RequestNotes :
	public x0::CustomData
{
	x0::DateTime ctime;
	Backend* backend;
	size_t retryCount;

	std::string bucketName;
	ClassfulScheduler::Bucket* bucket;

	explicit RequestNotes(x0::DateTime ct, Backend* b = nullptr) :
		ctime(ct),
		backend(b),
		retryCount(0),
		bucketName(),
		bucket(nullptr)
	{}

	~RequestNotes()
	{
		if (bucket) {
			bucket->put();
		}

		//if (backend_) {
		//	backend_->director().scheduler().release(backend_);
		//}
	}
};
