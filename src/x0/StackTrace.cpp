#include <x0/StackTrace.h>
#include <boost/lexical_cast.hpp>
#include <execinfo.h>

namespace x0 {

StackTrace::StackTrace(int numSkipFrames, int numMaxFrames)
{
	skip_ = numSkipFrames;
	symbols_ = NULL;
	addresses_ = new void *[numSkipFrames + numMaxFrames];
	count_ = backtrace(addresses_, numSkipFrames + numMaxFrames);
}

StackTrace::~StackTrace()
{
	delete[] addresses_;
	free(symbols_);
}

void StackTrace::generate()
{
	if (!symbols_)
	{
		// TODO: demangle C++-API when possible
		symbols_ = backtrace_symbols(addresses_ + skip_, count_);
	}
}

int StackTrace::length() const
{
	return count_;
}

const char *StackTrace::at(int index) const
{
	return symbols_[index];
}

const char *StackTrace::c_str() const
{
	const_cast<StackTrace *>(this)->generate();

	std::string buf;
	for (int i = 0; i < count_; ++i)
	{
		buf += " [";
		buf += boost::lexical_cast<std::string>(i);
		buf += "] ";
		buf += at(i);
		buf += "\n";
	}

	const_cast<StackTrace *>(this)->buf_ = buf;

	return buf_.c_str();
}


} // namespace x0
