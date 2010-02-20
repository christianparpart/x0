#ifndef sw_x0_file_hpp
#define sw_x0_file_hpp (1)

#include <x0/io/fileinfo.hpp>
#include <x0/types.hpp>
#include <string>
#include <memory>

namespace x0 {

/** file resource object.
 *
 * This file gets opened on object construction and will be automatically
 * closed when destructed.
 */
class X0_API file
{
private:
	fileinfo_ptr fileinfo_;
	int fd_;

public:
	/** opens a file.
	 * \param fi the file to open
	 * \param flags the file-open flags to pass to the system call.
	 * \see handle()
	 */
	explicit file(fileinfo_ptr fi, int flags = O_RDONLY);

	/** closes the file. */
	~file();

	/** the reference to the file information record. */
	fileinfo& info() const;

	/** the system's file descriptor to use to access this file resource. */
	int handle() const;
};

// {{{ inlines
inline fileinfo& file::info() const
{
	return *fileinfo_;
}

inline int file::handle() const
{
	return fd_;
}
// }}}

} // namespace x0

#endif
