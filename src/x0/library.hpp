#ifndef sw_x0_library_hpp
#define sw_x0_library_hpp (1)

#include <system_error>

namespace x0 {

class library
{
public:
	explicit library(const std::string& filename = std::string());
	library(library&& movable);
	~library();

	library& operator=(library&& movable);

	std::error_code open(const std::string& filename);
	bool open(const std::string& filename, std::error_code& ec);
	bool is_open() const;
	void *resolve(const std::string& symbol, std::error_code& ec);
	void close();

	void *operator[](const std::string& symbol);

private:
	void *handle_;
};

} // namespace x0

#endif
