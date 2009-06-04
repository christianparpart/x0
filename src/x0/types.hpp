/* <x0/types.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_types_h
#define x0_types_h (1)

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <cstring> // strrchr

namespace x0 {

/**
 * @defgroup common
 * @brief common APIs that are x0 neutral but developed within and for the x0 web server.
 */

/**
 * @defgroup core
 * @brief x0 web server core API.
 */

/**
 * @defgroup modules
 * @brief x0 web server modules.
 */

class connection;
struct request;
struct response;

typedef boost::shared_ptr<connection> connection_ptr;
typedef boost::shared_ptr<response> response_ptr;

/**
 * \ingroup common
 * \brief named enum `severity`, used by logging facility
 * \see logger
 */
struct severity {
	static const int emergency = 0;
	static const int alert = 1;
	static const int critical = 2;
	static const int error = 3;
	static const int warn = 4;
	static const int notice = 5;
	static const int info = 6;
	static const int debug = 7;

	int value_;
	severity(int value) : value_(value) { }
	explicit severity(const std::string& name);
	operator int() const { return value_; }
};

/**
 * \ingroup core
 * \brief request handler functor.
 */
typedef boost::function<void(request&, response&)> request_handler_fn;

#define __FILENAME__ ((std::strrchr(__FILE__, '/') ?: __FILE__ - 1) + 1)

} // namespace x0

#endif
