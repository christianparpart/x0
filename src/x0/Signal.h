/* <x0/signal.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_signal_hpp
#define x0_signal_hpp

#include <x0/Types.h>
#include <x0/Api.h>
#include <boost/next_prior.hpp>
#include <list>

namespace x0 {

//! \addtogroup base
//@{

/**
 * multi channel signal API.
 *
 * ...
 */
template<typename SignatureT> class signal;

/**
 * \brief signal API
 * \see handler<void(Args...)>
 */
template<typename... _Args>
class signal<void(_Args...)>
{
	signal(const signal&) = delete;
	signal& operator=(const signal&) = delete;

public:
	typedef boost::function<void(_Args...)> functor;
	typedef std::list<functor> list_type;

	typedef typename list_type::iterator iterator;
	typedef typename list_type::const_iterator const_iterator;

	typedef iterator connection;

public:
	signal() :
		impl_()
	{
	}

	~signal()
	{
	}

	bool empty() const
	{
		return impl_.empty();
	}

	std::size_t size() const
	{
		return impl_.size();
	}

	connection connect(const functor& fn)
	{
		impl_.push_back(fn);
		return boost::prior(impl_.end());
	}

	void disconnect(connection c)
	{
		impl_.erase(c);
	}

	void operator()(const _Args&&... args)
	{
		for (const_iterator i = impl_.begin(); i != impl_.end(); ++i)
		{
			(*i)(args...);
		}
	}

private:
	list_type impl_;
};

//@}

} // namespace x0

#endif
