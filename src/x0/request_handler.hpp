/* <x0/request_handler.hpp>
 * 
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_http_request_handler_hpp
#define x0_http_request_handler_hpp (1)

#include <x0/types.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <string>

namespace x0 {

struct request;
struct response;

/**
 * \ingroup core
 * \brief request handler functor.
 */
typedef function<void(request&, response&)> request_handler_fn;

} // namespace x0

#endif
