#include <x0/StackTrace.h>
#include <boost/lexical_cast.hpp>
#include <execinfo.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <strings.h>

namespace x0 {

StackTrace::StackTrace(int numSkipFrames, int numMaxFrames)
{
	skip_ = 1 + numSkipFrames;
	//buffer_.resize(numSkipFrames + numMaxFrames);
	addresses_ = new void *[skip_ + numMaxFrames];
	count_ = backtrace(addresses_, skip_ + numMaxFrames);
}

StackTrace::~StackTrace()
{
	delete[] addresses_;
}

inline auto stripLeftOf(const char *value, char ch) -> const char *
{
	const char *p = value;

	for (auto i = value; *i; ++i)
		if (*i == ch)
			p = i;

	return p != value ? p + 1 : p;
}

inline auto demangleSymbol(const char *symbolName, Buffer& result) -> void
{
	if (!symbolName || !*symbolName)
		result.push_back("<invalid symbol>");
	else
	{
		char *rv = 0;
		int status = 0;
		std::size_t len = 1024;

		try { rv = abi::__cxa_demangle(symbolName, result.end(), &len, &status); }
		catch (...) {}

		if (status < 0)
			result.push_back(symbolName);
		else
			result.resize(result.size() + strlen(rv));
	}
}

void StackTrace::generate(bool verbose)
{
	if (!symbols_.empty())
		return;

	for (int i = 0; i < count_; ++i)
	{
		void *address = addresses_[skip_ + i];

		buffer_.push_back('[');
		buffer_.push_back(i);
		buffer_.push_back("] ");

		std::size_t begin = buffer_.size();
		buffer_.reserve(buffer_.size() + 512);

		Dl_info info;
		if (!dladdr(address, &info))
			buffer_.push_back("<unresolved symbol>");
		else
		{
			demangleSymbol(info.dli_sname, buffer_);
			if (verbose)
			{
				buffer_.push_back(" in ");
				buffer_.push_back(stripLeftOf(info.dli_fname, '/'));
			}
		}

		std::size_t count = buffer_.size() - begin;
		symbols_.push_back(buffer_.ref(begin, count));

		buffer_.push_back("\n");
	}
}

/** retrieves the number of frames captured. */
int StackTrace::length() const
{
	return count_;
}

/** retrieves the frame information of a frame at given index. */
const BufferRef& StackTrace::at(int index) const
{
	return symbols_[index];
}

/** retrieves full stack trace information (of all captured frames). */
const char *StackTrace::c_str() const
{
	const_cast<StackTrace *>(this)->generate(true);

	return const_cast<StackTrace *>(this)->buffer_.c_str();
}

} // namespace x0
