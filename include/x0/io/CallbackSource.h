/* <CallbackSource.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

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
    virtual ssize_t sendto(Sink& sink);
    virtual const char* className() const;
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
