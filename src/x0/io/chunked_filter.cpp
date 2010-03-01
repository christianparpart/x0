#include <x0/io/chunked_filter.hpp>
#include <cassert>
#include <zlib.h>

namespace x0 {

chunked_filter::chunked_filter() :
	finished_(false)
{
}

buffer chunked_filter::process(const buffer_ref& input)
{
	buffer output;

	if (finished_)
		return output;

	if (input.empty())
		finished_ = true;

	char buf[12];
	int size = snprintf(buf, sizeof(buf), "%lx\r\n", input.size());

	output.push_back(buf, size);
	output.push_back(input);
	output.push_back("\r\n");

	//! \todo a filter might create multiple output-chunks, though, we could improve process() to not return the output but append them directly.
	// virtual void process(const buffer_ref& input, composite_source& output) = 0;

	return output;
}

} // namespace x0
