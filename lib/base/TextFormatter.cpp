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

void __f() {
	TextFormatter::print("Hello, %s", 42);
}

} // namespace x0
