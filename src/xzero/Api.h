// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#ifndef xzero_api_hpp
#define xzero_api_hpp (1)

#include <xzero/Defines.h>

// libxzero exports
#if defined(BUILD_XZERO_BASE_BASE)
#define XZERO_BASE_API XZERO_BASE_EXPORT
#else
#define XZERO_BASE_API XZERO_BASE_IMPORT
#endif

#endif
