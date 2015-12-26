// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/DataChain.h>
#include <xzero/io/FileView.h>
#include <xzero/Buffer.h>

namespace xzero {

DataChain::DataChain()
    : chunks_(),
      buffer_(),
      size_(0) {
}

void DataChain::reset() {
  chunks_.clear();
  buffer_.clear();
  size_ = 0;
}

bool DataChain::empty() const noexcept {
  return chunks_.empty() && buffer_.empty();
}

void DataChain::write(const char* cstr) {
  size_t clen = strlen(cstr);
  buffer_.push_back(cstr, clen);
  size_ += clen;
}

void DataChain::write(const char* buf, size_t n) {
  buffer_.push_back(buf, n);
  size_ += n;
}

void DataChain::write(const BufferRef& buf) {
  buffer_.push_back(buf.data(), buf.size());
  size_ += buf.size();
}

void DataChain::write(Buffer&& buf) {
  if (buf.size() < 1024) { // buffer to small to justify a new object?
    buffer_.push_back(buf);
    size_ += buf.size();
  } else {
    flushBuffer();
    size_ += buf.size();
    buffer_ = std::move(buf);
  }
}

void DataChain::write(FileView&& file) {
  flushBuffer();
  if (!file.empty()) {
    size_ += file.size();
    chunks_.emplace_back(new FileChunk(std::move(file)));
  }
}

void DataChain::write8(uint8_t bin) {
  buffer_.push_back((char) bin);
  size_ += 1;
}

void DataChain::write16(uint16_t bin) {
  buffer_.push_back((char) ((bin >> 8) & 0xFF));
  buffer_.push_back((char) (bin & 0xFF));
  size_ += 2;
}

void DataChain::write24(uint32_t bin) {
  buffer_.push_back((char) ((bin >> 16) & 0xFF));
  buffer_.push_back((char) ((bin >> 8) & 0xFF));
  buffer_.push_back((char) (bin & 0xFF));
  size_ += 3;
}

void DataChain::write32(uint32_t bin) {
  buffer_.push_back((char) ((bin >> 24) & 0xFF));
  buffer_.push_back((char) ((bin >> 16) & 0xFF));
  buffer_.push_back((char) ((bin >> 8) & 0xFF));
  buffer_.push_back((char) (bin & 0xFF));
  size_ += 4;
}

void DataChain::flushBuffer() {
  if (!buffer_.empty()) {
    chunks_.emplace_back(new BufferChunk(std::move(buffer_)));
  }
}

bool DataChain::transferTo(DataChainSink* target) {
  return transferTo(target, size_);
}

bool DataChain::transferTo(DataChainSink* target, size_t n) {
  flushBuffer();

  while (n > 0 && size_ > 0) {
    Chunk* front = chunks_.front().get();

    size_t transferred = front->transferTo(target, n);

    size_ -= transferred;
    n -= transferred;

    if (front->size() == 0) {
      chunks_.pop_front();
    }

    if (n == 0)
      return true;

    if (transferred < n)
      return false; // incomplete transfer
  }

  return n == 0;
}

// {{{ BufferChunk impl
size_t DataChain::BufferChunk::transferTo(DataChainSink* sink, size_t n) {
  size_t out = sink->transfer(buffer_.ref(offset_, std::min(n, size())));
  offset_ += out;
  return out;
}

size_t DataChain::BufferChunk::size() const {
  return buffer_.size() - offset_;
}
// }}}
// {{{ FileChunk impl
size_t DataChain::FileChunk::transferTo(DataChainSink* sink, size_t n) {
  size_t out = sink->transfer(file_.view(0, n));

  file_.setSize(file_.size() - out);
  file_.setOffset(file_.offset() + out);

  return out;
}

size_t DataChain::FileChunk::size() const {
  return file_.size();
}
// }}}

} // namespace xzero
