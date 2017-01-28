// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/DatagramConnector.h>

namespace xzero {

DatagramConnector::DatagramConnector(
    const std::string& name,
    DatagramHandler handler,
    Executor* executor)
    : name_(name),
      handler_(handler),
      executor_(executor) {
}

DatagramConnector::~DatagramConnector() {
}

DatagramHandler DatagramConnector::handler() const {
  return handler_;
}

void DatagramConnector::setHandler(DatagramHandler handler) {
  handler_ = handler;
}

} // namespace xzero
