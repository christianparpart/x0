/* <TextFormatter.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_TextFormatter_h
#define sw_x0_TextFormatter_h (1)

#include <x0/Buffer.h>
#include <memory>
#include <tuple>

namespace x0 {

template<typename Domain, typename... Args>
class X0_API TextFormatterImpl;

class X0_API TextFormatter
{
protected:
	Buffer* output_;

public:
	TextFormatter() : output_(nullptr) {}
	virtual ~TextFormatter() {}

	virtual void print(Buffer* output);

public:
	template<typename... Args>
	X0_API static std::shared_ptr<TextFormatterImpl<TextFormatter, Args...>> print(const char* fmt, Args... args) {
		return std::make_shared<TextFormatterImpl<TextFormatter, Args...>>(fmt, args...);
	}

	std::string str() { Buffer out; print(&out); return out.str(); }

	void setOutput(Buffer* output) { output_ = output; }
	Buffer* output() { return output_; }

	void print(char value, char fmt);
	void print(int value, char fmt);
	void print(long value, char fmt);
	void print(long long value, char fmt);
	void print(unsigned char value, char fmt);
	void print(unsigned int value, char fmt);
	void print(unsigned long value, char fmt);
	void print(unsigned long long value, char fmt);
	void print(const char* value, char fmt);
	void print(const Buffer& value, char fmt);
	void print(const BufferRef& value, char fmt);
};

template<typename Domain, typename... Args>
class X0_API TextFormatterImpl :
	public TextFormatter
{
private:
	Domain domain_;
	std::string format_;
	std::tuple<Args...> args_;

public:
	TextFormatterImpl(const std::string& format, Args... args) :
		TextFormatter(), format_(format), args_(std::move(args)...)
	{}

	Domain& domain() { return domain_; }
	const Domain& domain() const { return domain_; }

	template<const size_t i>
		typename std::enable_if<i == sizeof...(Args)>::type
	format(const char *s)
	{
		while (*s) {
			if (*s == '%' && *++s != '%')
				throw std::runtime_error("invalid format string: missing arguments");

			domain_.print(*s++, 0);
		}
	}

	template<const size_t i>
		typename std::enable_if<i != sizeof...(Args)>::type
	format(const char *s)
	{
		while (*s) {
			if (*s == '%' && *++s != '%') {
				domain_.print(std::get<i>(args_), *s);
				++s;
				format<i + 1>(s);
				return;
			}
			domain_.print(*s++, 0);
		}
		throw std::logic_error("extra arguments");
	}

	virtual void print(Buffer* output)
	{
		domain_.setOutput(output);
		format<0>(format_.c_str());
	}
};

X0_API inline Buffer& operator<<(Buffer& buf, std::shared_ptr<TextFormatter> formatter)
{
	formatter->print(&buf);
	return buf;
}

} // namespace x0

#endif
