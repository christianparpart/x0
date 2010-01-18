/* <x0/types.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_types_h
#define x0_types_h (1)

#include <x0/api.hpp>
#include <asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <cstring> // strrchr

namespace x0 {

/**
 * @defgroup base
 * @brief common base APIs that are x0 neutral but developed within and for the x0 web server and framework.
 */

/**
 * @defgroup core
 * @brief x0 web server core API.
 */

/**
 * @defgroup plugins
 * @brief x0 web server plugins.
 */

class fileinfo;
class connection;
struct request;
struct response;

struct file;
typedef std::shared_ptr<file> file_ptr;

//! \addtogroup core
//@{

typedef boost::shared_ptr<fileinfo> fileinfo_ptr;
typedef boost::shared_ptr<connection> connection_ptr;
typedef boost::shared_ptr<request> request_ptr;
typedef boost::shared_ptr<response> response_ptr;

/** request handler functor.
 */
typedef boost::function<void(request&, response&)> request_handler_fn;

//@}

} // namespace x0

#endif
