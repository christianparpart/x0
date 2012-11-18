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
#include <x0/Buffer.h>
#include <x0/BufferRef.h>
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
	X0_API static std::vector<T> tokenize(const T& input, const std::string& delimiter = " \t\r\n");

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

// {{{ impl
template<typename T>
inline Tokenizer<T>::Tokenizer(const T& input, const std::string& delimiter) :
	input_(input), token_(),
	lastPos_(0), charPos_(0), wordPos_(0), delimiter_(delimiter)
{
}

template<typename T>
inline const T& Tokenizer<T>::nextToken()
{
	if (end()) {
		static const T eos;
		return eos;
	}

	size_t isize = input_.size();
	lastPos_ = charPos_;

	while (charPos_ < isize && delimiter_.find(input_[charPos_]) == T::npos)
		++charPos_;

	token_ = substr(lastPos_, charPos_ - lastPos_);

	++wordPos_;
	lastPos_ = charPos_;

	return token_;
}

template<typename T>
inline void Tokenizer<T>::consumeDelimiter()
{
	size_t isize = input_.size();

	while (charPos_ < isize && delimiter_.find(input_[charPos_]) != T::npos) {
		++charPos_;
	}
}

template<typename T>
inline std::vector<T> Tokenizer<T>::tokenize()
{
	std::vector<T> tokens;

	while (!end())
		tokens.push_back(nextToken());

	return std::move(tokens);
}

template<>
inline std::string Tokenizer<std::string>::substr(size_t offset) const {
	return input_.substr(offset);
}

template<>
inline std::string Tokenizer<std::string>::substr(size_t offset, size_t size) const {
	return input_.substr(offset, size);
}

template<>
inline BufferRef Tokenizer<BufferRef>::substr(size_t offset, size_t size) const {
	return input_.ref(offset, size);
}

template<>
inline BufferRef Tokenizer<BufferRef>::substr(size_t offset) const {
	return input_.ref(offset);
}

template<typename T>
inline std::vector<T> Tokenizer<T>::tokenize(const T& input, const std::string& delimiter)
{
	Tokenizer<T> t(input, delimiter);
	return t.tokenize();
}

template<typename T>
inline bool Tokenizer<T>::end()
{
	consumeDelimiter();

	if (charPos_ >= input_.size())
		return true;

	return false;
}
// }}}

typedef Tokenizer<std::string> StringTokenizer;

} // namespace x0

#endif
