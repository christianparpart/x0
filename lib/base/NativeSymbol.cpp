// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/NativeSymbol.h>
#include <x0/Buffer.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <dlfcn.h>

namespace x0 {

NativeSymbol::NativeSymbol(const void* address) :
    symbol_()
{
    Dl_info info;
    if (dladdr(address, &info))
        symbol_ = info.dli_sname;
    else
        symbol_ = nullptr;
}

Buffer NativeSymbol::name() const
{
    Buffer buf;
    buf << *this;
    return buf;
}

inline auto stripLeftOf(const char *value, char ch) -> const char *
{
    const char *p = value;

    for (auto i = value; *i; ++i)
        if (*i == ch)
            p = i;

    return p != value ? p + 1 : p;
}

Buffer& operator<<(Buffer& result, const NativeSymbol& s)
{
    if (!s.native() || !*s.native())
        result.push_back("<invalid symbol>");
    else {
        char *rv = 0;
        int status = 0;
        std::size_t len = 2048;

        result.reserve(result.size() + len);

        try { rv = abi::__cxa_demangle(s.native(), result.end(), &len, &status); }
        catch (...) {}

        if (status < 0)
            result.push_back(s.native());
        else
            result.resize(result.size() + strlen(rv));

#if 0
        if (s.verbose()) {
            result.push_back(" in ");
            result.push_back(stripLeftOf(info.dli_fname, '/'));
        }
#endif
    }
    return result;
}

} // namespace x0
