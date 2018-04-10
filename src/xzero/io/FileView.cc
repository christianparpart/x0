// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/FileView.h>
#include <xzero/sysconfig.h>
#include <xzero/defines.h>
#include <system_error>
#include <stdexcept>
#include <cerrno>

#if defined(XZERO_OS_UNIX)
#include <unistd.h>
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

namespace xzero {

void FileView::read(Buffer* output) const {
  output->reserve(output->size() + size());

#if defined(HAVE_PREAD)
  ssize_t n = pread(handle(), output->end(), size(), offset());
  if (n < 0)
    throw std::system_error(errno, std::system_category());

  if (static_cast<size_t>(n) != size())
    throw std::runtime_error{"Did not read all required bytes from FileView."};

  output->resize(output->size() + n);
#else
  // TODO: implementation on windows
  throw std::runtime_error{ "FileView::read() not implemented on this platform." };
#endif
}

} // namespace xzero
