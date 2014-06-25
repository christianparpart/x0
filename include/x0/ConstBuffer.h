// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_ConstBuffer_h
#define sw_x0_ConstBuffer_h 1

#include <x0/Buffer.h>

namespace x0 {

class X0_API ConstBuffer : public Buffer
{
public:
    template<typename PodType, std::size_t N>
    explicit ConstBuffer(PodType (&value)[N]);

    ConstBuffer(const value_type *value, std::size_t n);

    virtual bool setCapacity(std::size_t n);
};

// {{{ ConstBuffer impl
template<typename PodType, std::size_t N>
inline ConstBuffer::ConstBuffer(PodType (&value)[N]) :
    Buffer()
{
    data_ = const_cast<char*>(value);
    size_ = N - 1;
    capacity_ = 0;
}

inline ConstBuffer::ConstBuffer(const value_type *value, std::size_t n) :
    Buffer(value, n)
{
    data_ = const_cast<char*>(value);
    size_ = n;
    capacity_ = 0;
}
// }}}

} // namespace x0

#endif
