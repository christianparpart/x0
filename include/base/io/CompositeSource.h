// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_CompositeSource_h
#define sw_x0_io_CompositeSource_h 1

#include <base/io/Source.h>
#include <base/Buffer.h>

#include <deque>

namespace base {

//! \addtogroup io
//@{

/** composite source.
 *
 * This source represents a sequential set of sub sources.
 * \see source
 */
class X0_API CompositeSource : public Source {
 private:
  typedef std::unique_ptr<Source> value_type;
  typedef std::deque<value_type> list_type;
  typedef list_type::iterator iterator;
  typedef list_type::const_iterator const_iterator;

 private:
  list_type sources_;

 public:
  CompositeSource();
  ~CompositeSource();

  bool empty() const;
  ssize_t size() const override;
  void push_back(std::unique_ptr<Source>&& s);
  template <typename T, typename... Args>
  T* push_back(Args... args);
  void clear();

  Source* front() const;
  void pop_front();

 public:
  virtual ssize_t sendto(Sink& sink);
  virtual const char* className() const;
};
//@}

// {{{ inlines
inline CompositeSource::CompositeSource() : Source(), sources_() {}

inline bool CompositeSource::empty() const { return sources_.empty(); }

inline ssize_t CompositeSource::size() const { return sources_.size(); }

inline void CompositeSource::push_back(std::unique_ptr<Source>&& s) {
  sources_.push_back(std::move(s));
}

template <typename T, typename... Args>
T* CompositeSource::push_back(Args... args) {
  T* chunk = new T(std::move(args)...);
  sources_.push_back(std::unique_ptr<T>(chunk));
  return chunk;
}

inline void CompositeSource::clear() { sources_.clear(); }

inline Source* CompositeSource::front() const { return sources_.front().get(); }

inline void CompositeSource::pop_front() { sources_.pop_front(); }
// }}}

}  // namespace base

#endif
