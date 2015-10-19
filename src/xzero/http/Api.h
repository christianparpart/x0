// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef xzero_http_api_hpp
#define xzero_http_api_hpp (1)

#include <xzero/Defines.h>

// libxzero/http exports
#if defined(BUILD_XZERO_BASE_HTTP)
#define XZERO_BASE_HTTP_API XZERO_BASE_EXPORT
#else
#define XZERO_BASE_HTTP_API XZERO_BASE_IMPORT
#endif

#endif
