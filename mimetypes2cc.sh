#! /bin/bash

if [[ $# -ne 3 ]]; then
  echo "Usage: $0 /path/to/mime.types output.cc export_symbol"
fi

INFILE="${1}"  # mime.types
OUTFILE="${2}" # mime.types.cc
SYMBOL="${3}"  # builtin_mimetypes

mkdir -p $(dirname ${OUTFILE}) || exit 1

#grep -v ^# $INFILE | grep -v ^$ | awk '
awk '
BEGIN {
  printf("#include <string>\n");
  printf("#include <unordered_map>\n");
  printf("\n");
  printf("std::unordered_map<std::string, std::string> '$SYMBOL' = {\n");
}

/^[^#].+$/ {
  if (NF >= 2) {
    for (i = 2; i <= NF; i += 1) {
      printf("  {\"%s\", \"%s\"},\n", $i, $1);
    }
  }
}

END {
  printf("};\n");
}' <$INFILE >$OUTFILE
