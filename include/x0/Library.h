/* <x0/Library.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_library_hpp
#define sw_x0_library_hpp (1)

#include <x0/Api.h>
#include <system_error>
#include <string>

namespace x0 {

class X0_API Library
{
public:
	explicit Library(const std::string& filename = std::string());
	Library(Library&& movable);
	~Library();

	Library& operator=(Library&& movable);

	std::error_code open(const std::string& filename);
	bool open(const std::string& filename, std::error_code& ec);
	bool isOpen() const;
	void *resolve(const std::string& symbol, std::error_code& ec);
	void close();

	void *operator[](const std::string& symbol);

private:
	void *handle_;
};

} // namespace x0

#endif
