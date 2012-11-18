/* <x0/Tokenizer.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_StringTokenizer_h
#define sw_x0_StringTokenizer_h

#include <x0/Api.h>
#include <string>
#include <vector>

namespace x0 {

template<typename T>
class X0_API Tokenizer {
private:
	T input_;
	T token_;

	mutable size_t lastPos_, charPos_;
	size_t wordPos_;
	std::string delimiter_;

public:
	explicit Tokenizer(const T& input, const std::string& delimiter = " \t\r\n");

	bool end();
	const T& nextToken();
	const T& token() const { return token_; }

	std::vector<T> tokenize();
	static std::vector<T> tokenize(const T& input, const std::string& delimiter = " \t\r\n");

	size_t charPosition() const { return charPos_; }
	size_t wordPosition() const { return lastPos_; }

	T gap() { end(); return charPos_ != lastPos_ ? substr(lastPos_, charPos_ - lastPos_) : T(); }
	T remaining() { return !end() ? substr(charPos_) : T(); }

	Tokenizer<T>& operator++() { nextToken(); return *this; }
	bool operator !() const { return const_cast<Tokenizer<T>*>(this)->end(); }

private:
	void consumeDelimiter();

	T substr(size_t offset) const;
	T substr(size_t offset, size_t size) const;
};

template<typename T>
inline Tokenizer<T>::Tokenizer(const T& input, const std::string& delimiter) :
	input_(input), token_(),
	lastPos_(0), charPos_(0), wordPos_(0), delimiter_(delimiter)
{
}

typedef Tokenizer<std::string> StringTokenizer;

} // namespace x0

#endif
