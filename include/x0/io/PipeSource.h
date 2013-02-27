/* <PipeSource.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_io_PipeSource_hpp
#define sw_x0_io_PipeSource_hpp 1

#include <x0/io/Pipe.h>
#include <x0/io/Source.h>
#include <x0/io/SinkVisitor.h>
#include <string>

namespace x0 {

//! \addtogroup io
//@{

/** file source.
 */
class X0_API PipeSource :
	public Source,
	public SinkVisitor
{
private:
	Pipe* pipe_;

	ssize_t result_;

public:
	explicit PipeSource(Pipe* pipe);
	~PipeSource();

	virtual ssize_t sendto(Sink& output);
	virtual const char* className() const;

protected:
	virtual void visit(BufferSink&);
	virtual void visit(FileSink&);
	virtual void visit(SocketSink&);
	virtual void visit(PipeSink&);
};

//@}

} // namespace x0

#endif
