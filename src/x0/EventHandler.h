/* <x0/event_handler.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_event_handler_hpp
#define sw_x0_event_handler_hpp (1)

#include <x0/Utility.h>
#include <x0/Api.h>

#include <functional>
#include <utility>
#include <memory>
#include <tuple>

namespace x0 {

template<typename SignatureT>
class event_handler;

/** \addtogroup base */
/*@{*/

/*! 
 * \brief Asynchronous Event Handler Dispatching API
 *
 * This class implements an asynchronous event handler dispatcher to allow the event handlers of an applications
 * to do other asynchronous operations (such as I/O) inside event handlers.
 *
 * Once the inner asynchronous operation did complete the event-handler informs about its completion by invoking
 * the function-object passed as first argument to it.
 *
 * This API could still see some improvements, as things like removing handlers or traversing, finding etc are 
 * currently not implemented.
 *
 * \code
 * void hook(const std::function<void()>& complete, int level, std::string msg) {
 *     std::clog << level << ": " << msg << std::endl;
 *     // ...
 *     complete();
 * }
 *
 * void footer(const std::function<void()>&, int level, std::string msg) {
 *     std::clog << "footer" << std::endl;
 *     complete();
 * }
 *
 * void test() {
 *     using namespace std::placeholders;
 *
 *     event_handler<int, std::string> handler;
 *
 *     handler.connect(std::bind(&hook, _1, _2, _3));
 *     handler.connect(std::bind(&footer, _1, _2, _3));
 *
 *     handler(42, "hello, world");
 * }
 * \endcode
 */
template<typename... Args>
class X0_API event_handler<void(Args...)>
{
public:
	struct X0_API node;
	class X0_API invokation_iterator;
	class X0_API connection;

	/** handlers who are to register to this event handling dispatcher must conform to this type.
	 *
	 * The first argument is the local completion handler to be invoked once this handler has finished its job
	 * and the remaining function arguments are the application supplied arguments.
	 */
	typedef std::function<void(invokation_iterator, Args...)> handler_type;

private:
	node *first_;		//!< link to the first event-handler node in list
	node *last_;		//!< link to the last event-handler node in list
	std::size_t size_;	//!< contains the number of event-handlers registered.

public:
	event_handler();
	~event_handler();

	std::size_t size() const;
	bool empty() const;

	connection connect(const handler_type& handler);
	void disconnect(connection con);

	void clear();

	event_handler& operator+=(const handler_type& handler);

	void operator()(Args&& ... args) const;
	void operator()(const std::function<void()>& handler, Args&& ... args) const;
};

// {{{ struct node
/** internal node containing a single event handler and a ptr to the next node if available, 
 * or nullptr otherwise. */
template<typename... Args>
struct X0_API event_handler<void(Args...)>::node
{
	handler_type handler_;
	node *next_;

	template<typename Sig> node(Sig&& fn) : handler_(fn), next_(0) {}
};
// }}}

// {{{ class invokation_iterator
/*!
 * \brief an object of this type is passed to each event handler as first argument to be invoked once completed.
 *
 * An invokation of this function object will trigger the next handler to be invoked or the initiators completion handler
 * if the last event handler has been invoked.
 */
template<typename... Args>
class X0_API event_handler<void(Args...)>::invokation_iterator
{
private:
	node *current_;								//!< current handler node to invoke
	std::shared_ptr<std::tuple<Args...>> args_;	//!< handler arguments passed from the initiator
	std::function<void()> handler_;				//!< completion handler

public:
	invokation_iterator() = delete;

	invokation_iterator(node *n, const std::shared_ptr<std::tuple<Args...>>& args) :
		current_(n),
		args_(args),
		handler_()
	{
	}

	invokation_iterator(node *n, const std::shared_ptr<std::tuple<Args...>>& args, const std::function<void()>& handler) :
		current_(n),
		args_(args),
		handler_(handler)
	{
	}

	/** invokes the handler this iterator points to and passes it a completion handler to the next node in list. */
	void operator()()
	{
		if (current_)
		{
			call_unpacked(current_->handler_, std::tuple_cat(std::make_tuple(next()), *args_));
		}
		else
		{
			done();
		}
	}

	/** invokes the completion handler (if passed) of this iteration. */
	void done()
	{
		if (handler_)
			handler_();
	}

private:
	/** creates the sibling iterator next to this that can be invoked as completion handler.
	 */
	invokation_iterator next()
	{
		return invokation_iterator(current_ ? current_->next_ : 0, args_, handler_);
	}
};
// }}}

// {{{ connection
template<typename... Args>
class X0_API event_handler<void(Args...)>::connection
{
private:
	event_handler<void(Args...)> *owner_;
	node *node_;

public:
	connection() :
		owner_(0), node_(0)
	{
	}

	connection(event_handler *o, node *n) :
		owner_(o), node_(n)
	{
	}

	connection(const connection&) = delete;
	connection& operator=(const connection& c) = delete;

	connection(connection&& c) :
		owner_(c.owner_), node_(c.node_)
	{
		c.owner_ = 0;
		c.node_ = 0;
	}

	connection& operator=(connection&& c)
	{
		owner_ = c.owner_;
		node_ = c.node_;

		c.owner_ = 0;
		c.node_ = 0;

		return *this;
	}

	~connection()
	{
		disconnect();
	}

	void detach()
	{
		owner_ = 0;
		node_ = 0;
	}

	void disconnect()
	{
		if (owner_)
		{
			if (node *n = owner_->first_)
			{
				if (n == node_)
				{
					owner_->first_ = n->next_;
					node_->next_ = 0;
					--owner_->size_;
					if (owner_->size_ == 0)
						owner_->last_ = 0;

					delete node_;
				}
				else while (n->next_)
				{
					if (n->next_ == node_)
					{
						n->next_ = node_->next_;
						node_->next_ = 0;
						--owner_->size_;
						if (node_ == owner_->last_)
							owner_->last_ = n;
						delete node_;
						break;
					}
				}
			}
			owner_ = 0;
			node_ = 0;
		}
	}
};
// }}}

// {{{ event_handler<void(Args...)> impl
template<typename... Args>
inline event_handler<void(Args...)>::event_handler() :
	first_(0),
	last_(0),
	size_(0)
{
}

template<typename... Args>
inline event_handler<void(Args...)>::~event_handler()
{
	clear();
}

template<typename... Args>
inline std::size_t event_handler<void(Args...)>::size() const
{
	return size_;
}

template<typename... Args>
inline bool event_handler<void(Args...)>::empty() const
{
	return !size_;
}

template<typename... Args>
inline typename event_handler<void(Args...)>::connection event_handler<void(Args...)>::connect(const handler_type& handler)
{
	if (last_)
	{
		last_->next_ = new node(handler);
		last_ = last_->next_;
	}
	else
	{
		first_ = last_ = new node(handler);
	}

	++size_;

	return connection(this, last_);
}

/** removes all callbacks from the list. */
template<typename... Args>
inline void event_handler<void(Args...)>::clear()
{
	while (first_)
	{
		node *next = first_->next_;
		delete first_;
		first_ = next;
	}

	last_ = 0;
	size_ = 0;
}

template<typename... Args>
inline event_handler<void(Args...)>& event_handler<void(Args...)>::operator+=(const handler_type& handler)
{
	connect(std::move(handler));
	return *this;
}

/** invokes all registered callbacks with the given arguments.
 *
 * \param args the arguments to pass to the registered callbacks.
 */
template<typename... Args>
inline void event_handler<void(Args...)>::operator()(Args&& ... args) const
{
	invokation_iterator(first_, std::make_shared<std::tuple<Args...>>(std::make_tuple(std::move(args)...)))();
}

/** invokes all registered callbacks with the given arguments.
 *
 * \param handler completion handler to invoke when every subscriber has been called.
 * \param args the arguments to pass to the registered callbacks.
 */
template<typename... Args>
inline void event_handler<void(Args...)>::operator()(const std::function<void()>& handler, Args&& ... args) const
{
	invokation_iterator(first_, std::make_shared<std::tuple<Args...>>(std::make_tuple(std::move(args)...)), handler)();
}
// }}}

/*@}*/

} // namespace x0

#endif
