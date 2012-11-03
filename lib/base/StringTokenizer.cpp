/* <x0/Tokenizer.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/StringTokenizer.h>
#include <x0/Buffer.h>
#include <x0/BufferRef.h>
#include <string>

namespace x0 {

template<typename T>
const T& Tokenizer<T>::nextToken()
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
void Tokenizer<T>::consumeDelimiter()
{
	size_t isize = input_.size();

	while (charPos_ < isize && delimiter_.find(input_[charPos_]) != T::npos) {
		++charPos_;
	}
}

template<typename T>
bool Tokenizer<T>::end()
{
	consumeDelimiter();

	if (charPos_ >= input_.size())
		return true;

	return false;
}

template<typename T>
std::vector<T> Tokenizer<T>::tokenize()
{
	std::vector<T> tokens;

	while (!end())
		tokens.push_back(nextToken());

	return std::move(tokens);
}

template<typename T>
std::vector<T> Tokenizer<T>::tokenize(const T& input, const std::string& delimiter)
{
	Tokenizer<T> t(input, delimiter);
	return t.tokenize();
}

template<>
std::string Tokenizer<std::string>::substr(size_t offset) const {
	return input_.substr(offset);
}

template<>
std::string Tokenizer<std::string>::substr(size_t offset, size_t size) const {
	return input_.substr(offset, size);
}

template<>
BufferRef Tokenizer<BufferRef>::substr(size_t offset, size_t size) const {
	return input_.ref(offset, size);
}

template<>
BufferRef Tokenizer<BufferRef>::substr(size_t offset) const {
	return input_.ref(offset);
}

template class Tokenizer<std::string>;
template class Tokenizer<BufferRef>;

} // namespace x0
