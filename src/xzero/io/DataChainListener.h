// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

namespace xzero {

class BufferRef;
class FileView;

/**
 * Interface to implement the other endpoint to transfer data out of a DataChain.
 *
 * Implement this interface if you want to splice your data
 * efficiently into a socket or pipe for example.
 */
class DataChainListener {
 public:
  virtual ~DataChainListener() {}

  virtual size_t transfer(const BufferRef& chunk) = 0;
  virtual size_t transfer(const FileView& chunk) = 0;
};

} // namespace xzero
