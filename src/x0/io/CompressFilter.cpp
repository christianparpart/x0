#include <x0/io/CompressFilter.h>
#include <cassert>

#include <zlib.h>

namespace x0 {

DeflateFilter::DeflateFilter(int level, bool raw) :
	CompressFilter(level),
	raw_(raw)
{
	z_.zalloc = Z_NULL;
	z_.zfree = Z_NULL;
	z_.opaque = Z_NULL;
}

DeflateFilter::DeflateFilter(int level) :
	CompressFilter(level),
	raw_(true)
{
}

Buffer DeflateFilter::process(const BufferRef& input, bool /*eof*/)
{
	if (input.empty())
		return Buffer();

	int rv = deflateInit2(&z_,
		level(),				// compression level
		Z_DEFLATED,				// method
		raw_ ? -15 : 15+16,		// window bits (15=gzip compression, 16=simple header, -15=raw deflate)
		9,						// memory level (1..9)
		Z_FILTERED				// strategy
	);

	if (rv != Z_OK)
		return Buffer(); // TODO throw error / inform caller about compression error

	z_.total_out = 0;
	z_.next_in = reinterpret_cast<Bytef *>(input.begin());
	z_.avail_in = input.size();

	Buffer output(input.size() * 1.1 + 12 + 18);
	z_.next_out = reinterpret_cast<Bytef *>(output.end());
	z_.avail_out = output.capacity();

	do
	{
		if (output.capacity() < output.size() / 2)
			output.reserve(output.capacity() + Buffer::CHUNK_SIZE);

		int rv = deflate(&z_, Z_FINISH);
		if (rv == Z_STREAM_ERROR)
		{
			deflateEnd(&z_);
			return Buffer();
		}
	}
	while (z_.avail_out == 0);

	assert(z_.avail_in == 0);

	output.resize(z_.total_out);

	deflateEnd(&z_);
	return output;
}

// --------------------------------------------------------------------------
BZip2Filter::BZip2Filter(int level) :
	CompressFilter(level)
{
	bz_.bzalloc = Z_NULL;
	bz_.bzfree = Z_NULL;
	bz_.opaque = Z_NULL;
}


Buffer BZip2Filter::process(const BufferRef& input, bool /*eof*/)
{
	if (input.empty())
		return Buffer();

	int rv = BZ2_bzCompressInit(&bz_,
		level(),						// compression level
		0,								// no output
		0								// work factor
	);

	if (rv != BZ_OK)
		return Buffer();


	bz_.next_in = input.begin();
	bz_.avail_in = input.size();
	bz_.total_in_lo32 = 0;
	bz_.total_in_hi32 = 0;

	Buffer output(input.size() * 1.1 + 12);
	bz_.next_out = output.end();
	bz_.avail_out = output.capacity();
	bz_.total_out_lo32 = 0;
	bz_.total_out_hi32 = 0;

	rv = BZ2_bzCompress(&bz_, BZ_FINISH);
	if (rv != BZ_STREAM_END)
	{
		BZ2_bzCompressEnd(&bz_);
		return Buffer();
	}

	if (bz_.total_out_hi32)
		return Buffer(); // file too large

	output.resize(bz_.total_out_lo32);

	rv = BZ2_bzCompressEnd(&bz_);
	if (rv != BZ_OK)
		return Buffer();

	return output;
}

} // namespace x0
