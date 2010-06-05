#include <x0/Library.h>
#include <x0/Defines.h>
#include <x0/sysconfig.h>

#include <dlfcn.h>

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
		DEBUG("error opening library '%s': %s", filename.c_str(), dlerror());
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

	return dlsym(handle_, symbol.c_str());
}

void *Library::operator[](const std::string& symbol)
{
	std::error_code ec;
	return resolve(symbol, ec);
}

void Library::close()
{
	if (handle_)
		dlclose(handle_);
}

} // namespace x0
