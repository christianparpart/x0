/* <FixedBufferSink.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */
#pragma once

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
class X0_API FixedBufferSink :
    public Sink
{
public:
    explicit FixedBufferSink(FixedBuffer& ref) :
        buffer_(ref)
    {
    }

    virtual void accept(SinkVisitor& v);
    virtual ssize_t write(const void *buffer, size_t size);

public:
    void clear()
    {
        buffer_.clear();
    }

    FixedBuffer& buffer()
    {
        return buffer_;
    }

    const FixedBuffer& buffer() const
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
    FixedBuffer buffer_;
};

//@}

} // namespace x0
