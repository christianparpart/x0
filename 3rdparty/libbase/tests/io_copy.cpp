// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/io/source.hpp>
#include <base/io/fd_source.hpp>
#include <base/io/file_source.hpp>

#include <base/io/sink.hpp>
#include <base/io/file_sink.hpp>

#include <base/io/filter.hpp>
#include <base/io/null_filter.hpp>
#include <base/io/uppercase_filter.hpp>
#include <base/io/CompressFilter.h>
#include <base/io/chain_filter.hpp>

#include <base/io/pump.hpp>

#include <iostream>
#include <memory>

#include <getopt.h>

inline base::file_ptr getfile(const std::string ifname) {
  return base::file_ptr(new base::File(base::FileInfoPtr(new base::FileInfo(ifname))));
}

int main(int argc, char *argv[]) {
  struct option options[] = {{"input", required_argument, 0, 'i'},
                             {"output", required_argument, 0, 'o'},
                             {"gzip", no_argument, 0, 'c'},
                             {"uppercase", no_argument, 0, 'U'},
                             {"help", no_argument, 0, 'h'},
                             {0, 0, 0, 0}};
  std::string ifname("-");
  std::string ofname("-");
  base::chain_filter cf;

  for (bool done = false; !done;) {
    int index = 0;
    int rv = (getopt_long(argc, argv, "i:o:hUc", options, &index));
    switch (rv) {
      case 'i':
        ifname = optarg;
        break;
      case 'o':
        ofname = optarg;
        break;
      case 'U':
        cf.push_back(base::filter_ptr(new base::uppercase_filter()));
        break;
      case 'c':
        cf.push_back(base::filter_ptr(new base::CompressFilter()));
        break;
      case 'h':
        std::cerr << "usage: " << argv[0] << " INPUT OUTPUT [-u]" << std::endl
                  << "  where INPUT and OUTPUT can be '-' to be interpreted as "
                     "stdin/stdout respectively." << std::endl;
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

  base::source_ptr input(ifname == "-" ? new base::fd_source(STDIN_FILENO)
                                     : new base::file_source(getfile(ifname)));

  base::sink_ptr output(ofname == "-" ? new base::fd_sink(STDOUT_FILENO)
                                    : new base::file_sink(ofname));

  pump(*input, *output, cf);

  return 0;
}
