/* <FilterSource.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_io_FilterSource_h
#define sw_x0_io_FilterSource_h 1

#include <x0/io/Source.h>
#include <x0/io/EmptySource.h>
#include <x0/io/Filter.h>
#include <memory>

namespace x0 {

//! \addtogroup io
//@{

/** puts a filter layer infront of the actual source.
 *
 * A filter might e.g. compress the source, embedd each chunk into chunked-encoding
 * or replace all FOOs with BARs.
 *
 * \see Source, Filter
 */
class X0_API FilterSource :
	public Source
{
public:
	explicit FilterSource(Filter* filter, bool force = false) :
		buffer_(), source_(new EmptySource()), filter_(filter), force_(force), pos_(0) {}

	FilterSource(Source* source, Filter* filter, bool force) :
		buffer_(), source_(source), filter_(filter), force_(force), pos_(0) {}

	~FilterSource();

	virtual ssize_t sendto(Sink& sink);
	virtual const char* className() const;

protected:
	Buffer buffer_;
	Source* source_;
	Filter* filter_;
	bool force_;
	size_t pos_;
};

//@}

} // namespace x0

#endif
