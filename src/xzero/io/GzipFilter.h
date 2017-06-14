// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/Api.h>
#include <xzero/io/Filter.h>
#include <string>

#include <zlib.h>

namespace xzero {

/**
 * Gzip encoding filter.
 */
class XZERO_BASE_API GzipFilter : public Filter {
 public:
  explicit GzipFilter(int level);
  ~GzipFilter();

  void filter(const BufferRef& input, Buffer* output, bool last) override;

 private:
  std::string z_code(int code) const;

 private:
  z_stream z_;
};

} // namespace xzero
