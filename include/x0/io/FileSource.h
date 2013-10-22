/* <FileSource.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_io_FileSource_hpp
#define sw_x0_io_FileSource_hpp 1

#include <x0/io/Source.h>
#include <x0/io/SinkVisitor.h>
#include <string>

namespace x0 {

//! \addtogroup io
//@{

/** file source.
 */
class X0_API FileSource :
	public Source,
	public SinkVisitor
{
private:
	int handle_;
	off_t offset_;
	size_t count_;
	bool autoClose_;

	ssize_t result_;

public:
	explicit FileSource(const char *filename);
	FileSource(int fd, off_t offset, std::size_t count, bool autoClose);
	~FileSource();

	inline int handle() const { return handle_; }

	virtual ssize_t sendto(Sink& output);
	virtual const char* className() const;

protected:
	virtual void visit(BufferSink&);
	virtual void visit(FileSink&);
	virtual void visit(SocketSink&);
	virtual void visit(PipeSink&);
	virtual void visit(SyslogSink&);
};

//@}

} // namespace x0

#endif
