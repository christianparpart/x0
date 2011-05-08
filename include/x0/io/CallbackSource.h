/* <CallbackSource.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
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
	typedef void (*Callback)(void*);

private:
	Callback callback_;
	void* data_;

public:
	explicit CallbackSource(Callback cb, void* data = nullptr);
	~CallbackSource();

public:
	virtual ssize_t sendto(Sink& sink);
	virtual const char* className() const;
};
//@}

// {{{ inlines
inline CallbackSource::CallbackSource(Callback cb, void* data) :
	Source(),
	callback_(cb),
	data_(data)
{
}
// }}}

} // namespace x0

#endif
