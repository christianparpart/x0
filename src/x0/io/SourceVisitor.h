/* <SourceVisitor.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_io_source_visitor_hpp
#define sw_x0_io_source_visitor_hpp 1

#include <x0/Api.h>

namespace x0 {

class SystemSource;
class FileSource;
class BufferSource;
class FilterSource;
class CompositeSource;

//! \addtogroup io
//@{

/** source visitor.
 *
 * \see source
 */
class X0_API SourceVisitor
{
public:
	virtual ~SourceVisitor() {}

	virtual void visit(SystemSource&) = 0;
	virtual void visit(FileSource&) = 0;
	virtual void visit(BufferSource&) = 0;
	virtual void visit(FilterSource&) = 0;
	virtual void visit(CompositeSource&) = 0;
};

//@}

} // namespace x0

#endif
