#include <x0/library.hpp>
#include <x0/defines.hpp>
#include <x0/sysconfig.h>

#include <dlfcn.h>

namespace x0 {

library::library(const std::string& filename) :
	handle_(0)
{
	if (!filename.empty())
		open(filename);
}

library::~library()
{
	if (is_open())
		close();
}

library::library(library&& movable) :
	handle_(movable.handle_)
{
	movable.handle_ = 0;
}

library& library::operator=(library&& movable)
{
	handle_ = movable.handle_;
	movable.handle_ = 0;

	return *this;
}

std::error_code library::open(const std::string& filename)
{
	std::error_code ec;
	open(filename, ec);
	return ec;
}

bool library::open(const std::string& filename, std::error_code& ec)
{
	ec.clear();
	handle_ = dlopen(filename.c_str(), RTLD_GLOBAL | RTLD_NOW);

	if (!handle_)
	{
		DEBUG("error opening library '%s': %s", filename.c_str(), dlerror());
		return false;
	}
	return true;
}

bool library::is_open() const
{
	return handle_ != 0;
}

void *library::resolve(const std::string& symbol, std::error_code& ec)
{
	if (!handle_)
		return 0;

	return dlsym(handle_, symbol.c_str());
}

void *library::operator[](const std::string& symbol)
{
	std::error_code ec;
	return resolve(symbol, ec);
}

void library::close()
{
	if (handle_)
		dlclose(handle_);
}

} // namespace x0
