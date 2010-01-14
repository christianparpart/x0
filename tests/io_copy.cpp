#include <x0/source.hpp>
#include <x0/file_source.hpp>
#include <x0/sink.hpp>
#include <x0/file_sink.hpp>
#include <x0/filter.hpp>
#include <x0/null_filter.hpp>
#include <x0/uppercase_filter.hpp>
#include <x0/compress_filter.hpp>
#include <x0/chain_filter.hpp>
#include <x0/pump.hpp>
#include <iostream>
#include <memory>
#include <getopt.h>

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

	std::shared_ptr<x0::source> input(ifname == "-" 
		? new x0::fd_source(STDIN_FILENO)
		: new x0::file_source(ifname)
	);

	std::shared_ptr<x0::sink> output(ofname == "-"
		? new x0::fd_sink(STDOUT_FILENO)
		: new x0::file_sink(ofname)
	);

	pump(*input, *output, cf);

	return 0;
}
