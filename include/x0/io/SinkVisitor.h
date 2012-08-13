/* <SinkVisitor.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_io_SinkVisitor_h
#define sw_x0_io_SinkVisitor_h 1

#include <x0/Api.h>

namespace x0 {

class BufferSink;
class FileSink;
class SocketSink;
class PipeSink;

//! \addtogroup io
//@{

/** source visitor.
 *
 * \see source
 */
class X0_API SinkVisitor
{
public:
	virtual ~SinkVisitor() {}

	virtual void visit(BufferSink&) = 0;
	virtual void visit(FileSink&) = 0;
	virtual void visit(SocketSink&) = 0;
	virtual void visit(PipeSink&) = 0;
};

//@}

} // namespace x0

#endif
