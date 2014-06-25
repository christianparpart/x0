// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_buffer_sink_hpp
#define sw_x0_io_buffer_sink_hpp 1

#include <x0/Buffer.h>
#include <x0/io/Sink.h>
#include <x0/io/Source.h>

namespace x0 {

//! \addtogroup io
//@{

/** sink, storing incoming data into a buffer.
 *
 * \see sink, source
 */
class X0_API BufferSink :
    public Sink
{
public:
    BufferSink() :
        buffer_()
    {
    }

    virtual void accept(SinkVisitor& v);
    virtual ssize_t write(const void *buffer, size_t size);

public:
    void clear()
    {
        buffer_.clear();
    }

    const Buffer& buffer() const
    {
        return buffer_;
    }

    std::size_t size() const
    {
        return buffer_.size();
    }

    bool empty() const
    {
        return buffer_.empty();
    }

private:
    Buffer buffer_;
};

//@}

} // namespace x0

#endif
