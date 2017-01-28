// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
    : chain_(),
      sink_(nullptr) {
}

EndPointWriter::~EndPointWriter() {
}

void EndPointWriter::write(const BufferRef& data) {
  TRACE("write: enqueue $0 bytes", data.size());
  chain_.write(data);
}

void EndPointWriter::write(Buffer&& chunk) {
  TRACE("write: enqueue $0 bytes", chunk.size());
  chain_.write(std::move(chunk));
}

void EndPointWriter::write(FileView&& chunk) {
  TRACE("write: enqueue $0 bytes", chunk.size());
  chain_.write(std::move(chunk));
}

bool EndPointWriter::flush(EndPoint* sink) {
  TRACE("write: flushing $0 bytes", chain_.size());
  sink_ = sink;
  return chain_.transferTo(this);
}

bool EndPointWriter::empty() const {
  return chain_.empty();
}

size_t EndPointWriter::transfer(const BufferRef& chunk) {
  TRACE("transfer(buf): $0 bytes", chunk.size());
  return sink_->flush(chunk);
}

size_t EndPointWriter::transfer(const FileView& file) {
  TRACE("transfer(file): $0 bytes, fd $1", file.size(), file.handle());
  return sink_->flush(file.handle(), file.offset(), file.size());
}

} // namespace xzero
