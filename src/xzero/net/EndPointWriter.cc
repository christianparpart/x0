// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/net/EndPointWriter.h>
#include <xzero/net/EndPoint.h>
#include <xzero/logging.h>
#include <unistd.h>

#ifndef NDEBUG
#define TRACE(msg...) logTrace("net.EndPointWriter", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

namespace xzero {

EndPointWriter::EndPointWriter()
    : chunks_() {
}

EndPointWriter::~EndPointWriter() {
}

void EndPointWriter::write(const BufferRef& data) {
  TRACE("write: enqueue $0 bytes", data.size());

  if (data.empty())
    return;

  chunks_.emplace_back(std::unique_ptr<Chunk>(new BufferRefChunk(data)));
}

void EndPointWriter::write(Buffer&& chunk) {
  TRACE("write: enqueue $0 bytes", chunk.size());

  if (chunk.empty())
    return;

  chunks_.emplace_back(std::unique_ptr<Chunk>(
        new BufferChunk(std::forward<Buffer>(chunk))));
}

void EndPointWriter::write(FileView&& chunk) {
  TRACE("write: enqueue $0 bytes", chunk.size());

  if (chunk.empty())
    return;

  chunks_.emplace_back(std::unique_ptr<Chunk>(
        new FileChunk(std::forward<FileView>(chunk))));
}

bool EndPointWriter::flush(EndPoint* sink) {
  TRACE("write: flushing $0 chunks", chunks_.size());

  while (!chunks_.empty()) {
    if (!chunks_.front()->transferTo(sink))
      return false;

    chunks_.pop_front();
  }

  return true;
}

// {{{ EndPointWriter::BufferChunk
bool EndPointWriter::BufferChunk::transferTo(EndPoint* sink) {
  size_t n = sink->flush(data_.ref(offset_));
  TRACE("BufferChunk.transferTo(): $0/$1 bytes written",
        n, data_.size() - offset_);

  offset_ += n;

  return offset_ == data_.size();
}
// }}}
// {{{ EndPointWriter::BufferRefChunk
bool EndPointWriter::BufferRefChunk::transferTo(EndPoint* sink) {
  size_t n = sink->flush(data_.ref(offset_));
  TRACE("BufferRefChunk.transferTo: $0 bytes", n);

  offset_ += n;
  return offset_ == data_.size();
}
// }}}
// {{{ EndPointWriter::FileChunk
EndPointWriter::FileChunk::~FileChunk() {
}

bool EndPointWriter::FileChunk::transferTo(EndPoint* sink) {
  const size_t n = sink->flush(file_.handle(), file_.offset(), file_.size());
  TRACE("FileChunk.transferTo(): $0/$1 bytes written", n, file_.size());

  file_.setSize(file_.size() - n);
  file_.setOffset(file_.offset() + n);

  return file_.size() == 0;
}
// }}}

} // namespace xzero
