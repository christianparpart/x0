/* <x0/Library.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/Library.h>
#include <x0/Defines.h>
#include <x0/sysconfig.h>

#include <vector>
#include <dlfcn.h>

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

public:
	dlfcn_error_category_impl()
	{
		vector_.push_back("Success");
	}

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

	virtual const char *name() const
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

dlfcn_error_category_impl& dlfcn_category() throw()
{
	static dlfcn_error_category_impl impl;
	return impl;
}
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
	handle_(movable.handle_)
{
	movable.handle_ = 0;
}

Library& Library::operator=(Library&& movable)
{
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
	ec.clear();
	handle_ = dlopen(filename.c_str(), RTLD_GLOBAL | RTLD_NOW);

	if (!handle_)
	{
		ec = dlfcn_category().make();
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

	if (!result)
		ec = dlfcn_category().make();
	else
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
	if (handle_)
	{
		dlclose(handle_);
		handle_ = 0;
	}
}

} // namespace x0
