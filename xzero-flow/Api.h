// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef flow_api_hpp
#define flow_api_hpp (1)

#include <xzero/defines.h>

// libxzero-flow exports
#if defined(BUILD_XZERO_FLOW)
#define XZERO_FLOW_API XZERO_EXPORT
#else
#define XZERO_FLOW_API XZERO_IMPORT
#endif

#endif
