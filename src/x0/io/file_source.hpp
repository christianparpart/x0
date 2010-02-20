#ifndef sw_x0_io_file_source_hpp
#define sw_x0_io_file_source_hpp 1

#include <x0/io/fd_source.hpp>
#include <x0/io/file.hpp>
#include <string>

namespace x0 {

//! \addtogroup io
//@{

/** file source.
 */
class X0_API file_source :
	public fd_source
{
private:
	file_ptr file_;

public:
	explicit file_source(const file_ptr& f);
	file_source(const file_ptr& f, std::size_t offset, std::size_t count);
	~file_source();

	virtual void accept(source_visitor& v);
};

//@}

} // namespace x0

#endif
