/* <x0/utility.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_event_handler_hpp
#define sw_x0_event_handler_hpp (1)

#include <x0/utility.hpp>
#include <boost/function.hpp>
#include <utility>
#include <tuple>

/** \addtogroup base */
/*@{*/

template<typename SignatureT>
class event_handler;

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
 * void hook(const boost::function<void()>& complete, int level, std::string msg) {
 *     std::clog << level << ": " << msg << std::endl;
 *     // ...
 *     complete();
 * }
 *
 * void handler_done() {
 *     std::clog << "handler done" << std::endl;
 * }
 *
 * void test() {
 *     event_handler<int, std::string> handler;
 *
 *     handler.connect(hook);
 *
 *     handler(handler_done, 42, "hello, world");
 * }
 * \endcode
 */
template<typename... Args>
class event_handler<void(Args...)>
{
public:
	/** handlers who are to register to this event handling dispatcher must conform to this type.
	 *
	 * The first argument is the local completion handler to be invoked once this handler has finished its job
	 * and the remaining function arguments are the application supplied arguments.
	 */
	typedef boost::function<void(const boost::function<void()>&, const Args&...)> handler_type;

private:
	/** internal node containing a single event handler and a ptr to the next node if available, 
	 * or nullptr otherwise. */
	struct node
	{
		handler_type handler_;
		node *next_;

		template<typename Sig> node(Sig&& fn) : handler_(fn), next_(0) {}
	};

public:
	/*!
	 * \brief an object of this type is passed to each event handler as first argument to be invoked once completed.
	 *
	 * An invokation of this function object will trigger the next handler to be invoked or the initiators completion handler
	 * if the last event handler has been invoked.
	 */
	class invocation_iterator
	{
	private:
		node *node_;						//!< current handler node to invoke
		boost::function<void()> handler_;	//!< initiator's completion handler
		std::tuple<Args...> args_;			//!< handler arguments passed from the initiator

	public:
		invocation_iterator() = delete;

		invocation_iterator(node *n, const boost::function<void()>& handler, const std::tuple<Args...>& args) :
			node_(n),
			handler_(std::move(handler)),
			args_(args)
		{
		}

		/** invokes the handler this iterator points to and passes it a completion handler to the next node in list. */
		void operator()()
		{
			if (node_)
			{
				call_unpacked(node_->handler_, std::tuple_cat(std::make_tuple(next()), args_));
			}
			else if (handler_) // completed invoking all handlers, so call initiators completion handler
			{
				handler_();
			}
		}

		/** creates the sibling iterator next to this that can be invoked as completion handler.
		 */
		invocation_iterator next()
		{
			return invocation_iterator(node_ ? node_->next_ : 0, handler_, args_);
		}
	};

private:
	node *first_;		//!< link to the first event-handler node in list
	node *last_;		//!< link to the last event-handler node in list
	std::size_t size_;	//!< contains the number of event-handlers registered.

public:
	event_handler();
	~event_handler();

	std::size_t size() const;
	bool empty() const;

	void connect(const handler_type& handler);
	void clear();

	event_handler& operator+=(const handler_type& handler);

	void operator()(const boost::function<void()>& completionHandler, const Args& ... args) const;
	void operator()(const Args& ... args) const;
};

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
inline void event_handler<void(Args...)>::connect(const handler_type& handler)
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
}

template<typename... Args>
inline void event_handler<void(Args...)>::clear()
{
	while (first_)
	{
		node *next = first_->next_;
		delete first_;
		first_ = next;
	}
}

template<typename... Args>
inline event_handler<void(Args...)>& event_handler<void(Args...)>::operator+=(const handler_type& handler)
{
	connect(std::move(handler));
	return *this;
}

template<typename... Args>
inline void event_handler<void(Args...)>::operator()(const boost::function<void()>& handler, const Args& ... args) const
{
	invocation_iterator(first_, std::move(handler), std::move(std::make_tuple(args...)))();
}

template<typename... Args>
inline void event_handler<void(Args...)>::operator()(const Args& ... args) const
{
	invocation_iterator(first_, std::move(boost::function<void()>()), std::move(std::make_tuple(args...)))();
}

/*@}*/

#endif
