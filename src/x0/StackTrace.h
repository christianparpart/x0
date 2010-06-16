#ifndef sw_x0_StackTrace_h
#define sw_x0_StackTrace_h 1

#include <x0/Api.h>
#include <x0/Buffer.h>
#include <x0/BufferRef.h>
#include <vector>
#include <string>

namespace x0 {

class X0_API StackTrace
{
private:
	Buffer buffer_;
	void **addresses_;
	std::vector<BufferRef> symbols_;
	int skip_;
	int count_;

public:
	explicit StackTrace(int numSkipFrames = 0, int numMaxFrames = 32);
	~StackTrace();

	void generate(bool verbose);

	int length() const;
	const BufferRef& at(int index) const;

	const char *c_str() const;
};

} // namespace x0

#endif
