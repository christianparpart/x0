#ifndef sw_x0_io_compress_filter_hpp
#define sw_x0_io_compress_filter_hpp 1

#include <x0/io/filter.hpp>
#include <x0/api.hpp>

#include <zlib.h>
#include <bzlib.h>

namespace x0 {

//! \addtogroup io
//@{

/** simply transforms all letters into upper-case letters (test filter).
 */
class X0_API compress_filter :
	public filter
{
public:
	explicit compress_filter(int level);

	int level() const;

private:
	int level_;
};

// {{{ compress_filter impl
inline compress_filter::compress_filter(int level) :
	filter(),
	level_(level)
{
	assert(level >= 0 && level <= 9);
}

inline int compress_filter::level() const
{
	return level_;
}
// }}}

// {{{ gzip_filter
/** deflate compression filter.
 */
class X0_API deflate_filter :
	public compress_filter
{
protected:
	deflate_filter(int level, bool gzip);

public:
	explicit deflate_filter(int level);

	virtual buffer process(const buffer_ref& data, bool eof = false);

private:
	z_stream z_;
	bool raw_;
};

/** gzip compression filter.
 */
class X0_API gzip_filter :
	public deflate_filter
{
public:
	gzip_filter(int level);
};

inline gzip_filter::gzip_filter(int level) :
	deflate_filter(level, false)
{
}
// }}}

// {{{ bzip2_filter
class bzip2_filter :
	public compress_filter
{
public:
	explicit bzip2_filter(int level);

	virtual buffer process(const buffer_ref& data, bool eof = false);

private:
	bz_stream bz_;
};
// }}}
//@}

} // namespace x0

#endif
