/* <HttpFileRef.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
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
        if (object_) {
            object_->ref();
        }
    }

    HttpFileRef(const HttpFileRef& v) :
        object_(v.object_)
    {
        if (object_) {
            object_->ref();
        }
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

    HttpFile* get() const { return object_; }

    HttpFile* operator->() { return object_; }
    const HttpFile* operator->() const { return object_; }

    bool operator!() const { return object_ == nullptr; }
    operator bool() const { return object_ != nullptr; }

    void reset()
    {
        if (object_) {
            object_->unref();
        }
        object_ = nullptr;
    }
};

} // namespace x0
