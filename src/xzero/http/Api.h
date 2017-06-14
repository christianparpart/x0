// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef xzero_http_api_hpp
#define xzero_http_api_hpp (1)

#include <xzero/defines.h>

// libxzero/http exports
#if defined(BUILD_XZERO_HTTP)
#define XZERO_HTTP_API XZERO_EXPORT
#else
#define XZERO_HTTP_API XZERO_IMPORT
#endif

#endif
