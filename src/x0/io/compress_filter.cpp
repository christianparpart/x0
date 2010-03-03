#include <x0/io/compress_filter.hpp>
#include <cassert>

#include <zlib.h>

namespace x0 {

deflate_filter::deflate_filter(int level, bool raw) :
	compress_filter(level),
	raw_(raw)
{
	z_.zalloc = Z_NULL;
	z_.zfree = Z_NULL;
	z_.opaque = Z_NULL;
}

deflate_filter::deflate_filter(int level) :
	compress_filter(level),
	raw_(true)
{
}

buffer deflate_filter::process(const buffer_ref& input)
{
	if (input.empty())
		return buffer();

	int rv = deflateInit2(&z_,
		level(),				// compression level
		Z_DEFLATED,				// method
		raw_ ? -15 : 15+16,		// window bits (15=gzip compression, 16=simple header, -15=raw deflate)
		9,						// memory level (1..9)
		Z_FILTERED				// strategy
	);

	if (rv != Z_OK)
		return buffer(); // TODO throw error / inform caller about compression error

	z_.total_out = 0;
	z_.next_in = reinterpret_cast<Bytef *>(input.begin());
	z_.avail_in = input.size();

	buffer output(input.size() * 1.1 + 12 + 18);
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

// --------------------------------------------------------------------------
bzip2_filter::bzip2_filter(int level) :
	compress_filter(level)
{
	bz_.bzalloc = Z_NULL;
	bz_.bzfree = Z_NULL;
	bz_.opaque = Z_NULL;
}


buffer bzip2_filter::process(const buffer_ref& input)
{
	if (input.empty())
		return buffer();

	int rv = BZ2_bzCompressInit(&bz_,
		level(),						// compression level
		0,								// no output
		0								// work factor
	);

	if (rv != BZ_OK)
		return buffer();


	bz_.next_in = input.begin();
	bz_.avail_in = input.size();
	bz_.total_in_lo32 = 0;
	bz_.total_in_hi32 = 0;

	buffer output(input.size() * 1.1 + 12);
	bz_.next_out = output.end();
	bz_.avail_out = output.capacity();
	bz_.total_out_lo32 = 0;
	bz_.total_out_hi32 = 0;

	rv = BZ2_bzCompress(&bz_, BZ_FINISH);
	if (rv != BZ_STREAM_END)
	{
		BZ2_bzCompressEnd(&bz_);
		return buffer();
	}

	if (bz_.total_out_hi32)
		return buffer(); // file too large

	output.resize(bz_.total_out_lo32);

	rv = BZ2_bzCompressEnd(&bz_);
	if (rv != BZ_OK)
		return buffer();

	return output;
}

} // namespace x0
