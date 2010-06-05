#ifndef sw_x0_file_hpp
#define sw_x0_file_hpp (1)

#include <x0/io/FileInfo.h>
#include <x0/Types.h>
#include <string>
#include <memory>

#include <fcntl.h> // O_RDONLY

namespace x0 {

/** file resource object.
 *
 * This file gets opened on object construction and will be automatically
 * closed when destructed.
 */
class X0_API File
{
private:
	FileInfoPtr fileinfo_;
	int fd_;

public:
	/** opens a file.
	 * \param fi the file to open
	 * \param flags the file-open flags to pass to the system call.
	 * \see handle()
	 */
	explicit File(FileInfoPtr fi, int flags = O_RDONLY);

	/** closes the file. */
	~File();

	/** the reference to the file information record. */
	FileInfo& info() const;

	/** the system's file descriptor to use to access this file resource. */
	int handle() const;
};

// {{{ inlines
inline FileInfo& File::info() const
{
	return *fileinfo_;
}

inline int File::handle() const
{
	return fd_;
}
// }}}

} // namespace x0

#endif
