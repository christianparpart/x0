/* <FileSource.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_io_FileSource_hpp
#define sw_x0_io_FileSource_hpp 1

#include <x0/io/SystemSource.h>
#include <x0/io/SinkVisitor.h>
#include <string>

namespace x0 {

//! \addtogroup io
//@{

/** file source.
 */
class X0_API FileSource :
	public SystemSource,
	public SinkVisitor
{
private:
	bool autoClose_;

	ssize_t result_;

public:
	explicit FileSource(const char *filename);
	FileSource(int fd, std::size_t offset, std::size_t count, bool autoClose);
	~FileSource();

	virtual void accept(SourceVisitor& v);

	virtual ssize_t sendto(Sink& output);

protected:
	virtual void visit(BufferSink&);
	virtual void visit(FileSink&);
	virtual void visit(SocketSink&);
};

//@}

} // namespace x0

#endif
