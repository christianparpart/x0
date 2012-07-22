/* <x0/StringTokenizer.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2011 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_StringTokenizer_h
#define sw_x0_StringTokenizer_h

#include <x0/Api.h>
#include <string>
#include <vector>

namespace x0 {

class X0_API StringTokenizer {
private:
	std::string input_;
	mutable int lastPos_, charPos_;
	int wordPos_;
	std::string delimiter_;
	bool exclusive_;
	mutable bool skipped_;
	char escapeChar_;
	std::string token_;

public:
	explicit StringTokenizer(const std::string& input, const std::string& delimiter, 
		char escapeChar = 0, bool exclusive = false);
	explicit StringTokenizer(const std::string& AInput);

	char escapeChar() const { return escapeChar_; }

	static std::string escape(const std::string& string, const std::string& delimiter, char escapeChar);

	const std::string& nextToken();

	const std::string& token() const { return token_; }

	int charPosition() const { return charPos_; }
	int wordPosition() const { return lastPos_; }

	bool end() const;

	std::string gap() const {
		end();
		return charPos_ != lastPos_ ? input_.substr(lastPos_, charPos_ - lastPos_) : std::string();
	}

	std::string remaining() const {
		return !end() ? input_.substr(charPos_) : std::string();
	}

	std::vector<std::string> tokenize();
};

} // namespace x0

#endif
