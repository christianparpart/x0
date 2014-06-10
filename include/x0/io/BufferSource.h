/* <BufferSource.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_io_BufferSource_hpp
#define sw_x0_io_BufferSource_hpp 1

#include <x0/Buffer.h>
#include <x0/io/Source.h>
#include <memory>

namespace x0 {

//! \addtogroup io
//@{

/** buffer source.
 *
 * \see Buffer, Source, Sink
 */
class X0_API BufferSource :
    public Source
{
public:
    BufferSource();
    template<typename PodType, std::size_t N> explicit BufferSource(PodType (&value)[N]);
    explicit BufferSource(const Buffer& data);
    explicit BufferSource(Buffer&& data);
    ~BufferSource();

    ssize_t size() const override;
    bool empty() const;

    ssize_t sendto(Sink& sink) override;
    const char* className() const override;

    Buffer& buffer() { return buffer_; }
    const Buffer& buffer() const { return buffer_; }

private:
    Buffer buffer_;
    std::size_t pos_;
};

//@}

// {{{ inlines
inline BufferSource::BufferSource() :
    buffer_(), pos_(0)
{
}

template<typename PodType, std::size_t N>
inline BufferSource::BufferSource(PodType (&value)[N]) :
    buffer_(), pos_(0)
{
    buffer_.push_back(value, N - 1);
}

inline BufferSource::BufferSource(const Buffer& data) :
    buffer_(data), pos_(0)
{
}

inline BufferSource::BufferSource(Buffer&& data) :
    buffer_(std::move(data)), pos_(0)
{
}

inline ssize_t BufferSource::size() const
{
    return buffer_.size() - pos_;
}

inline bool BufferSource::empty() const
{
    return size() == 0;
}
// }}}

} // namespace x0

#endif
