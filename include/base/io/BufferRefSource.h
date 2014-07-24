// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_BufferRefSource_hpp
#define sw_x0_io_BufferRefSource_hpp 1

#include <base/Buffer.h>
#include <base/io/Source.h>
#include <memory>

namespace base {

//! \addtogroup io
//@{

/** BufferRef source.
 *
 * \see BufferRef, Source, Sink
 */
class BASE_API BufferRefSource : public Source {
 public:
  BufferRefSource(const BufferRef& data);
  BufferRefSource(BufferRef&& data);
  ~BufferRefSource();

  ssize_t size() const override;
  bool empty() const;

  ssize_t sendto(Sink& sink) override;
  const char* className() const override;

 private:
  BufferRef buffer_;
  std::size_t pos_;
};

//@}

// {{{ inlines
inline BufferRefSource::BufferRefSource(const BufferRef& data)
    : buffer_(data), pos_(0) {}

inline BufferRefSource::BufferRefSource(BufferRef&& data)
    : buffer_(std::move(data)), pos_(0) {}

inline ssize_t BufferRefSource::size() const { return buffer_.size() - pos_; }

inline bool BufferRefSource::empty() const { return size() == 0; }
// }}}

}  // namespace base

#endif
