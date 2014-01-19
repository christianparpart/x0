/* <x0/Library.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/Library.h>
#include <x0/Defines.h>
#include <x0/DebugLogger.h>
#include <x0/sysconfig.h>

#include <vector>
#include <dlfcn.h>

#if !defined(XZERO_NDEBUG)
#	define TRACE(n, msg...) XZERO_DEBUG("Library", (n), msg)
#else
#	define TRACE(n, msg...) /*!*/ ((void)0)
#endif

// {{{ errors
enum class dlfcn_error
{
	success = 0,
	generic_error = 1
};

class dlfcn_error_category_impl :
	public std::error_category
{
private:
	std::vector<std::string> vector_;

#ifndef __APPLE__
public:
	dlfcn_error_category_impl()
	{
		vector_.push_back("Success");
	}
#endif

	std::error_code make()
	{
		if (const char *p = dlerror())
		{
			std::string msg(p);

			for (int i = 0, e = vector_.size(); i != e; ++i)
				if (vector_[i] == msg)
					return std::error_code(i, *this);

			vector_.push_back(msg);
			return std::error_code(vector_.size() - 1, *this);
		}
		return std::error_code(0, *this);
	}

	virtual const char *name() const noexcept(true)
	{
		return "dlfcn";
	}

	virtual std::string message(int ec) const
	{
		return vector_[ec];
	}
};

dlfcn_error_category_impl& dlfcn_category() throw();

namespace std {
	// implicit conversion from gai_error to error_code
	template<> struct is_error_code_enum<dlfcn_error> : public true_type {};
}

// ------------------------------------------------------------
#ifndef __APPLE__
dlfcn_error_category_impl& dlfcn_category() throw()
{
	static dlfcn_error_category_impl impl;
	return impl;
}
#endif
// }}}

namespace x0 {

Library::Library(const std::string& filename) :
	handle_(0)
{
	if (!filename.empty())
		open(filename);
}

Library::~Library()
{
	if (isOpen())
		close();
}

Library::Library(Library&& movable) :
    filename_(std::move(movable.filename_)),
	handle_(movable.handle_)
{
	movable.handle_ = 0;
}

Library& Library::operator=(Library&& movable)
{
    filename_ = std::move(movable.filename_);

	handle_ = movable.handle_;
	movable.handle_ = 0;

	return *this;
}

std::error_code Library::open(const std::string& filename)
{
	std::error_code ec;
	open(filename, ec);
	return ec;
}

bool Library::open(const std::string& filename, std::error_code& ec)
{
    filename_ = filename;
    TRACE(1, "open(): %s", filename_.c_str());

	ec.clear();
	handle_ = dlopen(filename.c_str(), RTLD_GLOBAL | RTLD_NOW);

	if (!handle_) {
#ifndef __APPLE__
		ec = dlfcn_category().make();
#endif
		return false;
	}

	return true;
}

bool Library::isOpen() const
{
	return handle_ != 0;
}

void *Library::resolve(const std::string& symbol, std::error_code& ec)
{
	if (!handle_)
		return 0;

	void *result = dlsym(handle_, symbol.c_str());

#ifndef __APPLE__
	if (!result)
		ec = dlfcn_category().make();
	else
#endif
		ec.clear();

	return result;
}

void *Library::operator[](const std::string& symbol)
{
	std::error_code ec;
	return resolve(symbol, ec);
}

void Library::close()
{
    TRACE(1, "close() %s", filename_.c_str());
	if (handle_)
	{
		dlclose(handle_);
		handle_ = 0;
	}
}

} // namespace x0
