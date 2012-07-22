/* <x0/StringTokenizer.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2011 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/StringTokenizer.h>
#include <x0/Buffer.h>

namespace x0 {

StringTokenizer::StringTokenizer(const std::string& input) :
	input_(input), lastPos_(0), charPos_(0), wordPos_(0), delimiter_(" \t\r\n"),
	exclusive_(false), skipped_(false), escapeChar_(0) {
}

StringTokenizer::StringTokenizer(const std::string& input,
	const std::string& delimiter, char escapeChar, bool exclusive) :
	input_(input), lastPos_(0), charPos_(0), wordPos_(0), delimiter_(delimiter),
	exclusive_(exclusive), skipped_(false), escapeChar_(escapeChar)
{
}

std::string StringTokenizer::escape(
	const std::string& string, 
	const std::string& delimiter,
	char escapeChar)
{
	Buffer result;

	result.reserve(string.size());

	for (const char *i = string.data(), *e = i + string.size(); i != e; ++i) {
		if (delimiter.find(*i) != std::string::npos)
			result << escapeChar;
		else if (*i == escapeChar)
			result << *i;

		result << *i;
	}

	return result.str();
}

const std::string& StringTokenizer::nextToken()
{
	if (end()) {
		static const std::string eos;
		return eos;
	}

	int isize = input_.size();

	skipped_ = false;
	if (escapeChar_) {
		lastPos_ = charPos_;
		token_ = std::string();
		char ch = input_[charPos_];
		while (charPos_ < isize && delimiter_.find(ch) == std::string::npos) {
			if (ch != escapeChar_)
				if (delimiter_.find(ch) == std::string::npos)
					token_ += ch;
				else
					break;
			else if (++charPos_ < isize)
				token_ += input_[charPos_];
			else
				token_ += ch;

			ch = input_[++charPos_];
		}
	} else {
		lastPos_ = charPos_;
		while (charPos_ < isize && delimiter_.find(input_[charPos_]) == std::string::npos)
			++charPos_;

		token_ = input_.substr(lastPos_, charPos_ - lastPos_);

		++wordPos_;
		lastPos_ = charPos_;
	}

	return token_;
}

bool StringTokenizer::end() const
{
	int save = charPos_;
	int isize = input_.size();

	if (!exclusive_)
		while (charPos_ < isize && delimiter_.find(input_[charPos_]) != std::string::npos)
			++charPos_;
	else {
		if (skipped_)
			return false;

		if (charPos_ < isize && delimiter_.find(input_[charPos_]) != std::string::npos)
			++charPos_;

		skipped_ = true;
	}

	bool reached = charPos_ >= isize;

	if (charPos_ != save && !reached)
		lastPos_ = save;

	return reached;
}

std::vector<std::string> StringTokenizer::tokenize()
{
	std::vector<std::string> tokens;

	while (!end())
		tokens.push_back(nextToken());

	return std::move(tokens);
}

} // namespace x0
