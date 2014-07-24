// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_BufferSource_hpp
#define sw_x0_io_BufferSource_hpp 1

#include <base/Buffer.h>
#include <base/io/Source.h>
#include <memory>

namespace base {

//! \addtogroup io
//@{

/** buffer source.
 *
 * \see Buffer, Source, Sink
 */
class BASE_API BufferSource : public Source {
 public:
  BufferSource();
  template <typename PodType, std::size_t N>
  explicit BufferSource(PodType (&value)[N]);
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
inline BufferSource::BufferSource() : buffer_(), pos_(0) {}

template <typename PodType, std::size_t N>
inline BufferSource::BufferSource(PodType (&value)[N])
    : buffer_(), pos_(0) {
  buffer_.push_back(value, N - 1);
}

inline BufferSource::BufferSource(const Buffer& data)
    : buffer_(data), pos_(0) {}

inline BufferSource::BufferSource(Buffer&& data)
    : buffer_(std::move(data)), pos_(0) {}

inline ssize_t BufferSource::size() const { return buffer_.size() - pos_; }

inline bool BufferSource::empty() const { return size() == 0; }
// }}}

}  // namespace base

#endif
