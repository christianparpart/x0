/* <src/TextFormatter.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */
#include <x0/TextFormatter.h>

namespace x0 {

void TextFormatter::print(Buffer* output) { }

void TextFormatter::print(char value, char /*fmt*/)
	{ *output_ << value; }

void TextFormatter::print(int value, char /*fmt*/)
	{ *output_ << value; }

void TextFormatter::print(long value, char /*fmt*/)
	{ *output_ << value; }

void TextFormatter::print(long long value, char /*fmt*/)
	{ *output_ << value; }

void TextFormatter::print(unsigned char value, char /*fmt*/)
	{ *output_ << value; }

void TextFormatter::print(unsigned int value, char /*fmt*/)
	{ *output_ << value; }

void TextFormatter::print(unsigned long value, char /*fmt*/)
	{ *output_ << value; }

void TextFormatter::print(unsigned long long value, char /*fmt*/)
	{ *output_ << value; }

void TextFormatter::print(const char* value, char /*fmt*/)
	{ *output_ << value; }

void TextFormatter::print(const Buffer& value, char /*fmt*/)
	{ *output_ << value; }

void TextFormatter::print(const BufferRef& value, char /*fmt*/)
	{ *output_ << value; }

void __f() {
	TextFormatter::print("Hello, %s", 42);
}

} // namespace x0
