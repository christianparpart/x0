#! /usr/bin/env python3
# This file is part of the "x0" project, http://github.com/christianparpart/x0>
#   (c) 2009-2017 Christian Parpart <christian@parpart.family>
#
# Licensed under the MIT License (the "License"); you may not use this
# file except in compliance with the License. You may obtain a copy of
# the License at: http://opensource.org/licenses/MIT

import sys

if len(sys.argv) != 4:
    print("Usage: {} /path/to/mime.types output.cc export_symbol".format(sys.argv[0]))
    exit(1)

INFILE = sys.argv[1]
OUTFILE = sys.argv[2]
SYMBOL = sys.argv[3]

# TODO mkdir -p $(dirname ${OUTFILE}) || exit 1

with open(INFILE) as f:
    with open(OUTFILE, 'w') as out:
        out.write("#include <string>\n");
        out.write("#include <unordered_map>\n");
        out.write("\n");
        out.write("std::unordered_map<std::string, std::string> {} = {{\n".format(SYMBOL));
        lines = f.readlines()
        for line in lines:
            line = line.rstrip('\n')
            cols = line.split()
            if len(cols) == 0 or cols[0].startswith('#'):
                continue
            mimetype = cols.pop(0)
            for ext in cols:
                out.write("  {{\"{}\", \"{}\"}},\n".format(ext, mimetype))
        out.write("};\n")
