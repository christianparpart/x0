/* <x0/ChunkedEncoder.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/ChunkedEncoder.h>
#include <cassert>
#include <zlib.h>

namespace x0 {

ChunkedEncoder::ChunkedEncoder() :
	finished_(false)
{
}

Buffer ChunkedEncoder::process(const BufferRef& input, bool eof)
{
#if 0
	DEBUG("ChunkedEncoder.proc: inputLen=%ld, eof=%d, finished=%d",
			input.size(), eof, finished_);

	if (input.empty() && !eof)
		return Buffer();

	Buffer output;

	if (input.size())
	{
		char buf[12];
		int size = snprintf(buf, sizeof(buf), "%lx\r\n", input.size());

		output.push_back(buf, size);
		output.push_back(input);
		output.push_back("\r\n");
	}

	if (eof)
	{
		output.push_back("0\r\n\r\n");
		finished_ = true;
	}

	//! \todo a filter might create multiple output-chunks, though, we could improve process() to not return the output but append them directly.
	// virtual void process(const BufferRef& input, composite_source& output) = 0;

	return output;
#else
	DEBUG("ChunkedEncoder.proc: inputLen=%ld", input.size());

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
#endif
}

} // namespace x0
