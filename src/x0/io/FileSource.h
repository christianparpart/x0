/* <FileSource.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_io_FileSource_hpp
#define sw_x0_io_FileSource_hpp 1

#include <x0/io/SystemSource.h>
#include <string>

namespace x0 {

//! \addtogroup io
//@{

/** file source.
 */
class X0_API FileSource :
	public SystemSource
{
private:
	bool autoClose_;

public:
	FileSource(int fd, std::size_t offset, std::size_t count, bool autoClose);
	~FileSource();

	virtual void accept(SourceVisitor& v);
};

//@}

} // namespace x0

#endif
