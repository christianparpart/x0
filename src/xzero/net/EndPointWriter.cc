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

namespace xzero {

EndPointWriter::EndPointWriter()
    : chain_(),
      sink_(nullptr) {
}

EndPointWriter::~EndPointWriter() {
}

void EndPointWriter::write(const BufferRef& data) {
  chain_.write(data);
}

void EndPointWriter::write(Buffer&& chunk) {
  chain_.write(std::move(chunk));
}

void EndPointWriter::write(FileView&& chunk) {
  chain_.write(std::move(chunk));
}

bool EndPointWriter::flushTo(TcpEndPoint* sink) {
  sink_ = sink;
  return chain_.transferTo(this);
}

bool EndPointWriter::flushTo(Buffer* sink) {
  return chain_.transferTo(sink);
}

bool EndPointWriter::empty() const {
  return chain_.empty();
}

size_t EndPointWriter::transfer(const BufferRef& chunk) {
  return sink_->write(chunk);
}

size_t EndPointWriter::transfer(const FileView& fileView) {
  return sink_->write(fileView);
}

} // namespace xzero
