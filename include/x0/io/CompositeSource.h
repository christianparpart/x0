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
	typedef Source* value_type;
	typedef std::deque<value_type> list_type;
	typedef list_type::iterator iterator;
	typedef list_type::const_iterator const_iterator;

private:
	list_type sources_;

public:
	CompositeSource();
	~CompositeSource();

	bool empty() const;
	void push_back(Source* s);
	template<typename T, typename... Args> T* push_back(Args... args);
	void reset();

	Source* front() const;
	void pop_front();

public:
	virtual ssize_t sendto(Sink& sink);
	virtual const char* className() const;
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

inline void CompositeSource::push_back(Source* s)
{
	sources_.push_back(s);
}

template<typename T, typename... Args>
T* CompositeSource::push_back(Args... args)
{
	T* chunk = new T(std::move(args)...);
	sources_.push_back(chunk);
	return chunk;
}

inline void CompositeSource::reset()
{
	for (auto i: sources_)
		delete i;

	sources_.clear();
}

inline Source* CompositeSource::front() const
{
	return sources_.front();
}

inline void CompositeSource::pop_front()
{
	sources_.pop_front();
}
// }}}

} // namespace x0

#endif
