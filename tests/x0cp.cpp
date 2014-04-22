#include <x0/io/FileSource.h>
#include <x0/io/FilterSource.h>
#include <x0/io/FileSink.h>

#include <x0/io/NullFilter.h>
#include <x0/io/ChainFilter.h>
#include <x0/io/CompressFilter.h>

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
    x0::ChainFilter cf;

    for (bool done = false; !done; ) {
        int index = 0;
        int rv = (getopt_long(argc, argv, "i:o:hgb", options, &index));
        switch (rv) {
            case 'i':
                ifname = optarg;
                break;
            case 'o':
                ofname = optarg;
                break;
            case 'g':
                cf.push_back(std::make_shared<x0::GZipFilter>(9));
                break;
            case 'b':
                cf.push_back(std::make_shared<x0::BZip2Filter>(9));
                break;
            case '?':
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

    x0::Source* input(ifname == "-" 
        ? new x0::FileSource("/dev/stdin")
        : new x0::FileSource(ifname.c_str())
    );

    x0::Sink* output(ofname == "-"
        ? new x0::FileSink("/dev/stdout", O_WRONLY)
        : new x0::FileSink(ofname, O_WRONLY | O_CREAT | O_TRUNC)
    );

    if (!cf.empty())
        input = new x0::FilterSource(input, &cf, true);

    ssize_t rv;
    size_t nwritten = 0;
    while ((rv = input->sendto(*output)) > 0)
        nwritten += rv;

    if (rv < 0)
        perror("copy error");

    std::cout << nwritten << " bytes written." << std::endl;

    return 0;
}
