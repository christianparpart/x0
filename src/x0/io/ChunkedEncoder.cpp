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
}

} // namespace x0
