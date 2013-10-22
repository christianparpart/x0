/* <x0/LogMessage.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Buffer.h>
#include <x0/Severity.h>
#include <x0/Api.h>

#include <deque>
#include <algorithm>

namespace x0 {

class X0_API LogMessage
{
public:
	LogMessage(Severity severity, const char* msg);

	template<typename Arg1, typename... Args>
	LogMessage(Severity severity, const char* msg, Arg1 arg1, Args... args);

	Severity severity() const { return severity_; }
	bool isError() const { return severity_ == Severity::error; }
	bool isWarning() const { return severity_ == Severity::warning; }
	bool isNotice() const { return severity_ == Severity::notice; }
	bool isInfo() const { return severity_ == Severity::info; }
	bool isDebug() const { return severity_ <= Severity::debug1; }

	BufferRef text() const { return tagBuffer_.ref(0, messageSize_); }

	bool hasTags() const { return !tags_.empty(); }
	size_t tagCount() const { return tags_.size(); }
	BufferRef tagAt(size_t i) const { return tagBuffer_.ref(tags_[i].first, tags_[i].second); }

	void addTag(const char* tag);
	void addTag(const std::string& tag);

	template<typename Arg1, typename... Args>
	void addTag(const char* fmt, Arg1 arg1, Args... args);

private:
	Severity severity_;
	Buffer tagBuffer_;
	size_t messageSize_;
	std::deque<std::pair<size_t, size_t>> tags_;
};

Buffer& operator<<(Buffer& output, const LogMessage& logMessage);

// {{{
inline LogMessage::LogMessage(Severity severity, const char* msg) :
	severity_(severity),
	tagBuffer_(),
	messageSize_(),
	tags_()
{
	tagBuffer_.push_back(msg);
	messageSize_ = tagBuffer_.size();
}

template<typename Arg1, typename... Args>
inline LogMessage::LogMessage(Severity severity, const char* msg, Arg1 arg1, Args... args) :
	severity_(severity),
	tagBuffer_(),
	messageSize_(),
	tags_()
{
	tagBuffer_.printf(msg, arg1, args...);
	messageSize_ = tagBuffer_.size();
}

inline void LogMessage::addTag(const char* tag)
{
	size_t begin = tagBuffer_.size();
	tagBuffer_.push_back(tag);
	tags_.push_front(std::make_pair(begin, tagBuffer_.size() - begin));
}

inline void LogMessage::addTag(const std::string& tag)
{
	size_t begin = tagBuffer_.size();
	tagBuffer_.push_back(tag);
	tags_.push_front(std::make_pair(begin, tagBuffer_.size() - begin));
}

template<typename Arg1, typename... Args>
inline void LogMessage::addTag(const char* fmt, Arg1 arg1, Args... args)
{
	size_t begin = tagBuffer_.size();
	tagBuffer_.printf(fmt, arg1, args...);
	tags_.push_front(std::make_pair(begin, tagBuffer_.size() - begin));
}

inline Buffer& operator<<(Buffer& output, const LogMessage& logMessage)
{
	if (logMessage.hasTags()) {
		for (size_t i = 0, e = logMessage.tagCount(); i != e; ++i) {
			output.push_back('[');
			output.push_back(logMessage.tagAt(i));
			output.push_back("] ");
		}
	}

	output.push_back(logMessage.text());

	return output;
}
// }}}

}
