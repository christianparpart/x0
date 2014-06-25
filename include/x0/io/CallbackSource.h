// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_CallbackSource_h
#define sw_x0_io_CallbackSource_h 1

#include <x0/io/Source.h>

#include <functional>

namespace x0 {

//! \addtogroup io
//@{

/** synthetic callback source, invoking a callback on \c sendto() invocation.
 *
 * \see Source
 */
class X0_API CallbackSource :
    public Source
{
public:
    typedef std::function<void()> Callback;

private:
    Callback callback_;

public:
    explicit CallbackSource(Callback cb);
    ~CallbackSource();

public:
    ssize_t size() const override;
    ssize_t sendto(Sink& sink) override;
    const char* className() const override;
};
//@}

// {{{ inlines
inline CallbackSource::CallbackSource(Callback cb) :
    Source(),
    callback_(cb)
{
}
// }}}

} // namespace x0

#endif
