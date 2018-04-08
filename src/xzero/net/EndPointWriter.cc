// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/EndPointWriter.h>
#include <xzero/net/TcpEndPoint.h>
#include <xzero/logging.h>
#include <unistd.h>

template<typename... Args> constexpr void TRACE(const char* msg, Args... args) {
#ifndef NDEBUG
  ::xzero::logTrace(std::string("net.EndPointWriter: ") + msg, args...);
#endif
}

namespace xzero {

EndPointWriter::EndPointWriter()
    : chain_(),
      sink_(nullptr) {
}

EndPointWriter::~EndPointWriter() {
}

void EndPointWriter::write(const BufferRef& data) {
  TRACE("write: enqueue {} bytes", data.size());
  chain_.write(data);
}

void EndPointWriter::write(Buffer&& chunk) {
  TRACE("write: enqueue {} bytes", chunk.size());
  chain_.write(std::move(chunk));
}

void EndPointWriter::write(FileView&& chunk) {
  TRACE("write: enqueue {} bytes", chunk.size());
  chain_.write(std::move(chunk));
}

bool EndPointWriter::flushTo(TcpEndPoint* sink) {
  TRACE("write: flushing {} bytes", chain_.size());
  sink_ = sink;
  return chain_.transferTo(this);
}

bool EndPointWriter::flushTo(Buffer* sink) {
  TRACE("write: flushing {} bytes", chain_.size());
  return chain_.transferTo(sink);
}

bool EndPointWriter::empty() const {
  return chain_.empty();
}

size_t EndPointWriter::transfer(const BufferRef& chunk) {
  TRACE("transfer(buf): {} bytes", chunk.size());
  return sink_->write(chunk);
}

size_t EndPointWriter::transfer(const FileView& fileView) {
  TRACE("transfer(file): {} bytes, fd {}", fileView.size(), fileView.handle());
  return sink_->write(fileView);
}

} // namespace xzero
