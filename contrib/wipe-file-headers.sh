#! /bin/bash

#for file in $(find src \( -name '*.h' -o -name '*.cc' \) -print); do
for file in ${@}; do
  cat $file | awk '
  BEGIN {
    in_header = 1;
  }
  /^\/\/.*/ {
  }
  /^$/ {
    if (in_header != 0) {
      in_header = 0;
    } else {
      print $0;
    }
  }
  /^[^\/].*/ {
    if (in_header != 0) {
      in_header = 0;
    } else {
      print $0;
    }
  }'
done


