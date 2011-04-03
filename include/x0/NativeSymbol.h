#ifndef sw_x0_NativeSymbol_h
#define sw_x0_NativeSymbol_h (1)

#include <x0/Api.h>

namespace x0 {

class Buffer;

class X0_API NativeSymbol
{
private:
	void* value_;
	bool verbose_;

public:
	explicit NativeSymbol(void* value, bool verbose = false) : value_(value), verbose_(verbose) {}

	void* value() const { return value_; }
	bool verbose() const { return verbose_; }
};

X0_API Buffer& operator<<(Buffer& b, const NativeSymbol& s);

} // namespace x0

#endif
