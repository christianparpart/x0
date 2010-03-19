#ifndef sw_x0_io_connection_sink_hpp
#define sw_x0_io_connection_sink_hpp 1

#include <x0/io/fd_sink.hpp>
#include <x0/io/source_visitor.hpp>
#include <x0/buffer.hpp>
#include <x0/types.hpp>

namespace x0 {

//! \addtogroup io
//@{

/** file descriptor stream sink.
 */
class X0_API connection_sink :
	public fd_sink,
	public source_visitor
{
public:
	explicit connection_sink(x0::connection *conn);

	x0::connection *connection() const;

	virtual pump_state pump(source& src);

public:
	virtual void visit(fd_source& v);
	virtual void visit(file_source& v);
	virtual void visit(buffer_source& v);
	virtual void visit(filter_source& v);
	virtual void visit(composite_source& v);

protected:
	x0::connection *connection_;
	pump_state rv_;
};

//@}

// {{{ impl
inline x0::connection *connection_sink::connection() const
{
	return connection_;
}
// }}}

} // namespace x0

#endif
