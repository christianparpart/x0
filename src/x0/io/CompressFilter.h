#ifndef sw_x0_io_compress_filter_hpp
#define sw_x0_io_compress_filter_hpp 1

#include <x0/io/Filter.h>
#include <x0/Api.h>

#include <zlib.h>
#include <bzlib.h>

namespace x0 {

//! \addtogroup io
//@{

/** simply transforms all letters into upper-case letters (test filter).
 */
class X0_API CompressFilter :
	public Filter
{
public:
	explicit CompressFilter(int level);

	int level() const;

private:
	int level_;
};

// {{{ CompressFilter impl
inline CompressFilter::CompressFilter(int level) :
	Filter(),
	level_(level)
{
	assert(level >= 0 && level <= 9);
}

inline int CompressFilter::level() const
{
	return level_;
}
// }}}

// {{{ GZipFilter
/** deflate compression filter.
 */
class X0_API DeflateFilter :
	public CompressFilter
{
protected:
	DeflateFilter(int level, bool gzip);

public:
	explicit DeflateFilter(int level);

	virtual Buffer process(const BufferRef& data, bool eof);

private:
	z_stream z_;
	bool raw_;
};

/** gzip compression filter.
 */
class X0_API GZipFilter :
	public DeflateFilter
{
public:
	GZipFilter(int level);
};

inline GZipFilter::GZipFilter(int level) :
	DeflateFilter(level, false)
{
}
// }}}

// {{{ BZip2Filter
class BZip2Filter :
	public CompressFilter
{
public:
	explicit BZip2Filter(int level);

	virtual Buffer process(const BufferRef& data, bool eof);

private:
	bz_stream bz_;
};
// }}}
//@}

} // namespace x0

#endif
