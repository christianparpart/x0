#! /bin/bash

HEADER=~/.vim.header

#for file in ${@}; do
for file in $(find src \( -name '*.h' -o -name '*.cc' \) -print); do
  cat $file | awk '
  BEGIN {
    in_header = 1;
  }
  {
    if ($0 ~ /^#/) {
      in_header = 0;
      print $0;
    } else if ($0 ~ /^[ ]*[a-zA-Z]/) {
      in_header = 0;
      print $0;
    } else if ($0 ~ /^$/) {
      if (in_header) {
        in_header = 0;
      } else {
        print $0;
      }
    } else if (in_header == 0) {
      print $0;
    }
  }
  ' > ${file}.noheader || exit $?
  rm ${file} || exit $?
  cat ${HEADER} ${file}.noheader > ${file} || exit $?
  rm -f ${file}.noheader
done


