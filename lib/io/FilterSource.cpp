// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/io/FilterSource.h>
#include <x0/io/BufferSink.h>
#include <x0/io/Filter.h>
#include <x0/Defines.h>
#include <memory>

namespace x0 {

FilterSource::~FilterSource()
{
    delete source_;
}

ssize_t FilterSource::sendto(Sink& sink)
{
    if (buffer_.empty()) {
        BufferSink input;

        ssize_t rv = source_->sendto(input);
        if (rv < 0 || (rv == 0 && !force_))
            return rv;

        buffer_ = (*filter_)(input.buffer().ref());
    }

    ssize_t result = sink.write(buffer_.data() + pos_, buffer_.size() - pos_);

    if (result > 0) {
        pos_ += result;
        if (pos_ == buffer_.size()) {
            pos_ = 0;
            buffer_.clear();
        }
    }

    return result;
}

ssize_t FilterSource::size() const
{
    return source_->size();
}

const char* FilterSource::className() const
{
    return "FilterSource";
}

} // namespace x0
