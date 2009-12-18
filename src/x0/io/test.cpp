#include <x0/io/chunk.hpp>
#include <x0/io/source.hpp>
#include <x0/io/sink.hpp>
#include <x0/io/filter.hpp>
#include <x0/io/null_filter.hpp>
#include <x0/io/file_source.hpp>
#include <x0/io/file_sink.hpp>

int main(int argc, const char *argv[])
{
	x0::file_source input("test.cpp");
	x0::file_sink output("output.txt");

//	auto nfc = x0::chain(x0::null_filter(), x0::null_filter());
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
