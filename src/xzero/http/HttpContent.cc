// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#include <xzero/http/HttpContent.h>
#include <xzero/io/FileUtil.h>

namespace xzero {
namespace http {


// {{{ HttpContent::Builder impl
HttpContent::Builder::Builder(size_t bufferSize)
    : bufferSize_(bufferSize),
      size_(0),
      buffer_(),
      fd_() {
}

void HttpContent::Builder::write(const BufferRef& chunk) {
  if (buffer_.size() + chunk.size() > bufferSize_)
    tryDisplaceBufferToFile();

  if (fd_.isOpen())
    FileUtil::write(fd_, chunk);
  else
    buffer_.push_back(chunk);
}

void HttpContent::Builder::write(FileView&& chunk) {
  tryDisplaceBufferToFile();

  if (fd_.isOpen())
    FileUtil::write(fd_, chunk);
  else
    FileUtil::read(chunk, &buffer_);
}

HttpContent&& HttpContent::Builder::commit() {
  return fd_.isOpen()
      ? std::move(HttpContent(std::move(fd_), size_))
      : std::move(HttpContent(std::move(buffer_)));
}

void HttpContent::Builder::tryDisplaceBufferToFile() {
  if (fd_.isClosed()) {
    fd_ = FileUtil::createTempFile();
  }
}
// }}}

} // namespace http
} // namespace xzero
