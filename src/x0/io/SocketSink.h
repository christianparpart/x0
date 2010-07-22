/* <SocketSink.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_io_SocketSink_hpp
#define sw_x0_io_SocketSink_hpp 1

#include <x0/io/Sink.h>
#include <x0/io/SourceVisitor.h>
#include <x0/Socket.h>
#include <x0/Buffer.h>
#include <x0/Types.h>

namespace x0 {

class Socket;

//! \addtogroup io
//@{

/** file descriptor stream sink.
 */
class X0_API SocketSink :
	public Sink,
	public SourceVisitor
{
public:
	explicit SocketSink(Socket *conn);

	Socket *socket() const;
	void setSocket(Socket *);

	virtual ssize_t pump(Source& src);

public: // SourceVisitor
	virtual void visit(SystemSource& v);
	virtual void visit(FileSource& v);
	virtual void visit(BufferSource& v);
	virtual void visit(FilterSource& v);
	virtual void visit(CompositeSource& v);

	ssize_t genericPump(Source& v);

protected:
	Socket *socket_;
	Buffer buf_;
	off_t offset_;
	ssize_t result_;
	std::error_code errorCode_;
};

//@}

// {{{ impl
inline Socket *SocketSink::socket() const
{
	return socket_;
}

inline void SocketSink::setSocket(Socket *value)
{
	socket_ = value;
}
// }}}

} // namespace x0

#endif
