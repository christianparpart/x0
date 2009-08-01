/* <x0/handler.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_handler_hpp
#define x0_handler_hpp

#include <x0/types.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/next_prior.hpp>
#include <list>

namespace x0 {

/** \addtogroup core */
/*@{*/

/**
 * multi channel handler API.
 *
 * when being invoked, it calls all handlers being registered with this handler
 * until the first feels responsible (that is: returns true).
 */
class handler :
	public boost::noncopyable
{
public:
	typedef boost::function<bool(request&, response&)> functor;
	typedef std::list<functor> list_type;
	typedef list_type::iterator iterator;
	typedef list_type::const_iterator const_iterator;
	typedef list_type::iterator connection;

public:
	handler() :
		impl_()
	{
	}

	~handler()
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

	bool operator()(request& in, response& out)
	{
		for (const_iterator i = impl_.begin(); i != impl_.end(); ++i)
		{
			if ((*i)(in, out))
			{
				return true;
			}
		}
		return false;
	}

private:
	list_type impl_;
};

/*@}*/

} // namespace x0

#endif
