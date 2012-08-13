/* <x0/ChunkedEncoder.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/ChunkedEncoder.h>
#include <x0/StackTrace.h>
#include <cassert>
#include <zlib.h>

#if 0 // !defined(NDEBUG)
#	define TRACE(msg...) DEBUG("ChunkedEncoder: " msg)
#else
#	define TRACE(msg...)
#endif

#define ERROR(msg...) do { \
	TRACE(msg); \
	StackTrace st; \
	TRACE("Stack Trace:\n%s", st.c_str()); \
} while (0)

namespace x0 {

ChunkedEncoder::ChunkedEncoder() :
	finished_(false)
{
}

Buffer ChunkedEncoder::process(const BufferRef& input)
{
#if 0
	if (input.empty())
		ERROR("proc: EOF");
	else
		TRACE("proc: inputLen=%ld", input.size());
#endif

	if (finished_)
		return Buffer();

	if (input.empty())
		finished_ = true;

	Buffer output;

	if (input.size())
	{
		char buf[12];
		int size = snprintf(buf, sizeof(buf), "%lx\r\n", input.size());

		output.push_back(buf, size);
		output.push_back(input);
		output.push_back("\r\n");
	}
	else
		output.push_back("0\r\n\r\n");

	//! \todo a filter might create multiple output-chunks, though, we could improve process() to not return the output but append them directly.
	// virtual void process(const BufferRef& input, composite_source& output) = 0;

	return output;
}

} // namespace x0
