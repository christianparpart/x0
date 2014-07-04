// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_ChainFilter_hpp
#define sw_x0_io_ChainFilter_hpp 1

#include <x0/io/Filter.h>
#include <memory>
#include <deque>

namespace x0 {

//! \addtogroup io
//@{

/** chaining filter API, supporting sub filters to be chained together.
 */
class X0_API ChainFilter : public Filter {
 public:
  virtual Buffer process(const BufferRef& input);

 public:
  void push_front(FilterPtr f);
  void push_back(FilterPtr f);
  void clear();

  std::size_t size() const;
  bool empty() const;

  const FilterPtr& operator[](std::size_t index) const;

 private:
  std::deque<FilterPtr> filters_;
};

//{{{ inlines impl
inline void ChainFilter::push_front(FilterPtr f) { filters_.push_front(f); }

inline void ChainFilter::push_back(FilterPtr f) { filters_.push_back(f); }

inline void ChainFilter::clear() { filters_.clear(); }

inline std::size_t ChainFilter::size() const { return filters_.size(); }

inline bool ChainFilter::empty() const { return filters_.empty(); }

inline const FilterPtr& ChainFilter::operator[](std::size_t index) const {
  return filters_[index];
}
//}}}

//@}

}  // namespace x0

#endif
