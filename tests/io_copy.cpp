#include <x0/io/source.hpp>
#include <x0/io/fd_source.hpp>
#include <x0/io/file_source.hpp>

#include <x0/io/sink.hpp>
#include <x0/io/file_sink.hpp>

#include <x0/io/filter.hpp>
#include <x0/io/null_filter.hpp>
#include <x0/io/uppercase_filter.hpp>
#include <x0/io/compress_filter.hpp>
#include <x0/io/chain_filter.hpp>

#include <x0/io/pump.hpp>

#include <iostream>
#include <memory>

#include <getopt.h>

inline x0::file_ptr getfile(const std::string ifname)
{
	return x0::file_ptr(new x0::file(
		x0::fileinfo_ptr(new x0::fileinfo(ifname))));
}

int main(int argc, char *argv[])
{
	struct option options[] = {
		{ "input", required_argument, 0, 'i' },
		{ "output", required_argument, 0, 'o' },
		{ "gzip", no_argument, 0, 'c' },
		{ "uppercase", no_argument, 0, 'U' },
		{ "help", no_argument, 0, 'h' },
		{ 0, 0, 0, 0 }
	};
	std::string ifname("-");
	std::string ofname("-");
	x0::chain_filter cf;

	for (bool done = false; !done; )
	{
		int index = 0;
		int rv = (getopt_long(argc, argv, "i:o:hUc", options, &index));
		switch (rv)
		{
			case 'i':
				ifname = optarg;
				break;
			case 'o':
				ofname = optarg;
				break;
			case 'U':
				cf.push_back(x0::filter_ptr(new x0::uppercase_filter()));
				break;
			case 'c':
				cf.push_back(x0::filter_ptr(new x0::compress_filter()));
				break;
			case 'h':
				std::cerr
					<< "usage: " << argv[0] << " INPUT OUTPUT [-u]" << std::endl
					<< "  where INPUT and OUTPUT can be '-' to be interpreted as stdin/stdout respectively." << std::endl;
				return 0;
			case 0:
				break;
			case -1:
				done = true;
				break;
			default:
				std::cerr << "syntax error: "
						  << "(" << rv << ")" << std::endl;
				return 1;
		}
	}

	x0::source_ptr input(ifname == "-" 
		? new x0::fd_source(STDIN_FILENO)
		: new x0::file_source(getfile(ifname))
	);

	x0::sink_ptr output(ofname == "-"
		? new x0::fd_sink(STDOUT_FILENO)
		: new x0::file_sink(ofname)
	);

	pump(*input, *output, cf);

	return 0;
}
