/* <HttpFileRef.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <x0/http/HttpFile.h>

namespace x0 {

// XXX I may consider replacing this with std::shared_ptr<> if performance is appropriate in a single threaded context.

class X0_API HttpFileRef {
private:
	HttpFile* object_;

public:
	HttpFileRef() : object_(nullptr) {}

	HttpFileRef(HttpFile* f) :
		object_(f)
	{
		object_->ref();
	}

	HttpFileRef(const HttpFileRef& v) :
		object_(v.object_)
	{
		object_->ref();
	}

	HttpFileRef& operator=(const HttpFileRef& v)
	{
		HttpFile* old = object_;

		object_ = v.object_;
		object_->ref();

		if (old)
			old->unref();

		return *this;
	}

	~HttpFileRef()
	{
		if (object_) {
			object_->unref();
		}
	}
		
	HttpFile* operator->()
	{
		return object_;
	}
};

} // namespace x0
