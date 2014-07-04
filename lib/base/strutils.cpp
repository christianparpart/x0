// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/strutils.h>
#include <x0/Buffer.h>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <cctype>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace x0 {

Buffer readFile(const std::string& filename) {
  Buffer result;

  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) return result;

  for (;;) {
    char buf[4096];
    ssize_t nread = ::read(fd, buf, sizeof(buf));
    if (nread <= 0) break;

    result.push_back(buf, nread);
  }

  ::close(fd);

  return result;
}

std::string trim(const std::string& value) {
  std::size_t left = 0;
  while (std::isspace(value[left])) ++left;

  std::size_t right = value.size() - 1;
  while (std::isspace(value[right])) --right;

  return value.substr(left, 1 + right - left);
}

}  // namespace x0
