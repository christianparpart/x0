#ifndef sw_x0_StackTrace_h
#define sw_x0_StackTrace_h 1

#include <x0/Api.h>
#include <string>

namespace x0 {

class X0_API StackTrace
{
private:
	void **addresses_;
	char **symbols_;
	int skip_;
	int count_;
	std::string buf_;

public:
	explicit StackTrace(int numSkipFrames = 0, int numMaxFrames = 32);
	~StackTrace();

	void generate();

	int length() const;
	const char *at(int index) const;

	const char *c_str() const;
};

} // namespace x0

#endif
