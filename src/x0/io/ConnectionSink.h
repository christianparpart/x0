#ifndef sw_x0_io_ConnectionSink_hpp
#define sw_x0_io_ConnectionSink_hpp 1

#include <x0/io/SystemSink.h>
#include <x0/io/SourceVisitor.h>
#include <x0/Buffer.h>
#include <x0/Types.h>

namespace x0 {

class HttpConnection;

//! \addtogroup io
//@{

/** file descriptor stream sink.
 */
class X0_API ConnectionSink :
	public SystemSink,
	public SourceVisitor
{
public:
	explicit ConnectionSink(HttpConnection *conn);

	HttpConnection *connection() const;

	virtual ssize_t pump(Source& src);

public:
	virtual void visit(SystemSource& v);
	virtual void visit(FileSource& v);
	virtual void visit(BufferSource& v);
	virtual void visit(FilterSource& v);
	virtual void visit(CompositeSource& v);

protected:
	HttpConnection *connection_;
	ssize_t rv_;
};

//@}

// {{{ impl
inline HttpConnection *ConnectionSink::connection() const
{
	return connection_;
}
// }}}

} // namespace x0

#endif
