#include <x0/io/source.hpp>
#include <x0/io/file_source.hpp>
#include <x0/io/sink.hpp>
#include <x0/io/file_sink.hpp>
#include <x0/io/filter.hpp>
#include <x0/io/null_filter.hpp>
#include <x0/io/uppercase_filter.hpp>
#include <x0/io/chain_filter.hpp>
#include <x0/io/pump.hpp>
#include <iostream>
#include <memory>

int main(int argc, const char *argv[])
{
	if (argc != 3 && argc != 4)
	{
		std::cerr << "usage: " << argv[0] << " INPUT OUTPUT [-u]" << std::endl;
		std::cerr << "  where INPUT and OUTPUT can be '-' to be interpreted as stdin/stdout respectively." << std::endl;
		return 1;
	}

	std::string ifn(argv[1]);
	std::shared_ptr<x0::source> input(ifn == "-" 
		? new x0::fd_source(STDIN_FILENO)
		: new x0::file_source(ifn)
	);

	std::string ofn(argv[2]);
	std::shared_ptr<x0::sink> output(ofn == "-"
		? new x0::fd_sink(STDOUT_FILENO)
		: new x0::file_sink(ofn)
	);

	if (argc >= 4 && std::string(argv[3]) == "-u")
	{
		x0::chain_filter cf;
		cf.push_back(x0::filter_ptr(new x0::uppercase_filter()));

		pump(*input, *output, cf);
	}
	else
	{
		pump(*input, *output);
	}

	return 0;
}
