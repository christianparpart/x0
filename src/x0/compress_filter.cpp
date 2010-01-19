#include <x0/compress_filter.hpp>
#include <cassert>
#include <zlib.h>

namespace x0 {

compress_filter::compress_filter()
{
	z_.zalloc = Z_NULL;
	z_.zfree = Z_NULL;
	z_.opaque = Z_NULL;
}

buffer compress_filter::process(const buffer_ref& input)
{
	if (input.empty())
		return buffer();

	int rv = deflateInit2(&z_,
		Z_DEFAULT_COMPRESSION,	// compression level
		Z_DEFLATED,				// method
		15 + 16,				// window bits
		9,						// memory level (1..9)
		Z_FILTERED				// strategy
	);

	if (rv != Z_OK)
		return buffer(); // TODO throw error / inform caller about compression error

	z_.total_out = 0;
	z_.next_in = reinterpret_cast<Bytef *>(input.begin());
	z_.avail_in = input.size();

	buffer output(input.size());
	z_.next_out = reinterpret_cast<Bytef *>(output.end());
	z_.avail_out = output.capacity();

	do
	{
		if (output.capacity() < output.size() / 2)
			output.reserve(output.capacity() + buffer::CHUNK_SIZE);

		int rv = deflate(&z_, Z_FINISH);
		if (rv == Z_STREAM_ERROR)
		{
			deflateEnd(&z_);
			return buffer();
		}
	}
	while (z_.avail_out == 0);

	assert(z_.avail_in == 0);

	output.resize(z_.total_out);

	deflateEnd(&z_);
	return output;
}

} // namespace x0
