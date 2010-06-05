/* <x0/request_handler.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_request_handler_hpp
#define x0_request_handler_hpp

#include <x0/Types.h>
#include <x0/Api.h>
#include <x0/EventHandler.h>

namespace x0 {

class HttpRequest;
class HttpResponse;

//! \addtogroup core
//@{

/**
 * asynchronous handler API.
 *
 * when being invoked, it calls all handlers being registered with this request_handler
 * until the first feels responsible (that is: returns true).
 */
class request_handler :
	public event_handler<void(HttpRequest *, HttpResponse *)>
{
private:
	typedef event_handler<void(HttpRequest *, HttpResponse *)> base_type;

public:
	request_handler() : base_type()
	{
	}

	template<typename T> connection connect(T f)
	{
		using namespace std::placeholders;

		return base_type::connect(std::bind(f, _1, _2, _3));
	}

	template<typename T, typename U> connection connect(T f, U u)
	{
		using namespace std::placeholders;

		return base_type::connect(std::bind(f, u, _1, _2, _3));
	}
};

//@}

} // namespace x0

#endif
