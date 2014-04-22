/* <NativeSymbol.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_NativeSymbol_h
#define sw_x0_NativeSymbol_h (1)

#include <x0/Api.h>
#include <typeinfo>

namespace x0 {

class Buffer;

class X0_API NativeSymbol
{
private:
    const char* symbol_;

public:
    explicit NativeSymbol(const void* address);

    explicit NativeSymbol(const char* symbol) :
        symbol_(symbol)
    {}

    template<typename T>
    explicit NativeSymbol() :
        symbol_(typeid(T).name())
    {}

    const char* native() const { return symbol_; }

    Buffer name() const;
};

X0_API Buffer& operator<<(Buffer& b, const NativeSymbol& s);

} // namespace x0

#endif
