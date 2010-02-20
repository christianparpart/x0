#ifndef sw_x0_io_source_visitor_hpp
#define sw_x0_io_source_visitor_hpp 1

#include <x0/api.hpp>

namespace x0 {

class fd_source;
class file_source;
class buffer_source;
class filter_source;
class composite_source;

//! \addtogroup io
//@{

/** source visitor.
 *
 * \see source
 */
class X0_API source_visitor
{
public:
	virtual ~source_visitor() {}

	virtual void visit(fd_source&) = 0;
	virtual void visit(file_source&) = 0;
	virtual void visit(buffer_source&) = 0;
	virtual void visit(filter_source&) = 0;
	virtual void visit(composite_source&) = 0;
};

//@}

} // namespace x0

#endif
