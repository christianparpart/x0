#include <x0/io/source.hpp>
#include <x0/io/sink.hpp>
#include <x0/io/filter.hpp>
#include <x0/io/null_filter.hpp>
#include <x0/io/file_source.hpp>
#include <x0/io/file_sink.hpp>

int main(int argc, const char *argv[])
{
	const int MAX_ITERS = 3;
	x0::buffer buf;

	for (int i = 1; i <= MAX_ITERS; ++i)
	{
		std::printf("iteration# %d/%d:\n", i, MAX_ITERS);

		const char *ifn = argc == 2 ? argv[1] : argv[0];
		x0::file_source input(ifn);
		input.async(true);

		char ofn[1024];
		snprintf(ofn, sizeof(ofn), "%s.%d.bak", ifn, i);
		x0::file_sink output(ofn);
		output.async(true);

		buf.clear();

		while (x0::buffer::view chunk = input.pull(buf))
		{
			while (chunk = output.push(chunk))
				std::printf(" did just a partial write: %ld bytes remaining\n", chunk.size());

			std::printf("  buffer: size=%ld (capacity %ld)\n", buf.size(), buf.capacity());
		}
	}

	//null_filter nf;

//	auto nfc = chain(null_filter(), null_filter());
//	nfc.all(input, output);

//-------------------------------------------------

	// configure filter
//	filter f;
//	f.chain(file_source("input.txt"));
//	f.chain(uppercase_encoder());
//	f.chain(base64_encoder());
//	f.chain(file_sink("output.txt"));

	// pump input source through the filter path, store result into file output.txt.
//	f.all(file_source("input.txt"), file_sink("output.txt"));

//	f.all();
//	f.once();
//	f.async();

	return 0;
}
