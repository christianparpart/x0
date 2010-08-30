/* <CompositeSource.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_io_CompositeSource_h
#define sw_x0_io_CompositeSource_h 1

#include <x0/io/Source.h>
#include <x0/Buffer.h>
#include <x0/io/SourceVisitor.h>

#include <deque>

namespace x0 {

//! \addtogroup io
//@{

/** composite source.
 *
 * This source represents a sequential set of sub sources.
 * \see source
 */
class X0_API CompositeSource :
	public Source
{
private:
	typedef SourcePtr value_type;
	typedef std::deque<value_type> list_type;
	typedef list_type::iterator iterator;
	typedef list_type::const_iterator const_iterator;

private:
	list_type sources_;

public:
	CompositeSource();

	bool empty() const;
	void push_back(SourcePtr s);
	void reset();

	//std::size_t size() const;
	//SourcePtr operator[](std::size_t index) const;

public:
	virtual BufferRef pull(Buffer& output);
	virtual void accept(SourceVisitor& v);
};

//@}

// {{{ inlines
inline CompositeSource::CompositeSource() :
	Source(),
	sources_()
{
}

inline bool CompositeSource::empty() const
{
	return sources_.empty();
}

inline void CompositeSource::push_back(SourcePtr s)
{
	sources_.push_back(s);
}

inline void CompositeSource::reset()
{
	sources_.clear();
}

//inline std::size_t CompositeSource::size() const
//{
//	return sources_.size();
//}

//inline SourcePtr CompositeSource::operator[](std::size_t index) const
//{
//	return sources_[index];
//}
// }}}

} // namespace x0

#endif
