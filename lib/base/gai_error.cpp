/* <x0/gai_error.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/Defines.h>
#include <x0/gai_error.h>

#include <netdb.h>

namespace x0 {

class gai_error_category_impl :
	public std::error_category
{

	virtual const char *name() const noexcept(true)
	{
		return "gai";
	}

	virtual std::string message(int ec) const
	{
		return gai_strerror(ec);
	}
};

#ifndef __APPLE__
gai_error_category_impl gai_category_impl_;

const std::error_category& gai_category() throw()
{
	return gai_category_impl_;
}
#endif

} // namespace x0
