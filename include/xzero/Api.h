// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef xzero_api_hpp
#define xzero_api_hpp (1)

#include <base/Defines.h>

// libxzero exports
#if defined(BUILD_XZERO)
#define XZERO_API BASE_EXPORT
#else
#define XZERO_API BASE_IMPORT
#endif

#endif
