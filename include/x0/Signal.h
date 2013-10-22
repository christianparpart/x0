/* <Signal.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef x0_signal_hpp
#define x0_signal_hpp

#include <x0/Types.h>
#include <x0/Api.h>
#include <list>
#include <algorithm>

namespace x0 {

//! \addtogroup base
//@{

/**
 * multi channel signal API.
 *
 * ...
 */
template<typename SignatureT> class Signal;

/**
 * \brief signal API
 * \see handler<void(Args...)>
 */
template<typename... Args>
class Signal<void(Args...)>
{
	Signal(const Signal&) = delete;
	Signal& operator=(const Signal&) = delete;

public:
	typedef std::pair<void *, void(*)(void *, Args...)> item_type;
	typedef std::list<item_type> list_type;

	typedef typename list_type::iterator iterator;
	typedef typename list_type::const_iterator const_iterator;

	typedef iterator Connection;

public:
	Signal() :
		impl_()
	{
	}

	~Signal()
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

	template<class K, void (K::*method)(Args...)>
	static void method_thunk(void *object, Args... args)
	{
		(static_cast<K *>(object)->*method)(args...);
	}

	template<class K, void (K::*method)(Args...)>
	Connection connect(K *object)
	{
		impl_.push_back(std::make_pair(object, &method_thunk<K, method>));
		auto handle = impl_.end();
		--handle;
		return handle;
	}

	void disconnect(void* p)
	{
		std::remove_if(impl_.begin(), impl_.end(), [&](const item_type& i) { return i.first == p; });
	}

	void disconnect(Connection c)
	{
		impl_.erase(c);
	}

	void operator()(Args... args) const
	{
		for (auto i: impl_) {
			(*i.second)(i.first, args...);
		}
	}

	void clear()
	{
		impl_.clear();
	}

private:
	list_type impl_;
};

//@}

} // namespace x0

#endif
