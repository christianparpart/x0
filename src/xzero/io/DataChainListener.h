// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
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
