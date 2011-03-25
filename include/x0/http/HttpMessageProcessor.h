/* <HttpMessageProcessor.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_message_processor_h
#define sw_x0_message_processor_h (1)

#include <x0/Buffer.h>
#include <x0/BufferRef.h>
#include <x0/io/ChainFilter.h>
#include <x0/Defines.h>
#include <x0/Api.h>

#include <system_error>
#include <functional>
#include <cctype>

//! \addtogroup http
//@{

// {{{ enum class HttpMessageError
namespace x0 {
	enum class HttpMessageError
	{
		/** the request has been fully parsed, including possibly existing request body. */
		Success = 0,
		/** the request chunk passed has been successsfully parsed, but the request is incomplete (partial). */
		Partial,
		/** the request has been parsed up to a point where a callback raised an "abort-parsing"-notice. */
		Aborted,
		/** a syntax error occurred while parsing the request chunk. */
		SyntaxError
	};

	const std::error_category& http_message_category() throw();

	std::error_code make_error_code(HttpMessageError ec);
	std::error_condition make_error_condition(HttpMessageError ec);
}

namespace std {
	// implicit conversion from HttpMessageError to error_code
	template<> struct is_error_code_enum<x0::HttpMessageError> : public true_type {};
}

// {{{ inlines
namespace x0 {
	inline std::error_code make_error_code(HttpMessageError ec)
	{
		return std::error_code(static_cast<int>(ec), x0::http_message_category());
	}

	inline std::error_condition make_error_condition(HttpMessageError ec)
	{
		return std::error_condition(static_cast<int>(ec), x0::http_message_category());
	}
}
// }}}
// }}}

namespace x0 {

/**
 * \class HttpMessageProcessor
 * \brief implements an HTTP/1.1 (request/response) message parser and processor
 */
class HttpMessageProcessor
{
public:
	enum ParseMode { // {{{
		REQUEST,
		RESPONSE,
		MESSAGE
	}; // }}}

	enum State { // {{{
		// artificial
		SYNTAX_ERROR = 1,
		MESSAGE_BEGIN,

		// Request-Line
		REQUEST_LINE_BEGIN = 100,
		REQUEST_METHOD,
		REQUEST_ENTITY_BEGIN,
		REQUEST_ENTITY,
		REQUEST_PROTOCOL_BEGIN,
		REQUEST_PROTOCOL_T1,
		REQUEST_PROTOCOL_T2,
		REQUEST_PROTOCOL_P,
		REQUEST_PROTOCOL_SLASH,
		REQUEST_PROTOCOL_VERSION_MAJOR,
		REQUEST_PROTOCOL_VERSION_MINOR,
		REQUEST_LINE_LF,

		// Status-Line
		STATUS_LINE_BEGIN = 150,
		STATUS_PROTOCOL_BEGIN,
		STATUS_PROTOCOL_T1,
		STATUS_PROTOCOL_T2,
		STATUS_PROTOCOL_P,
		STATUS_PROTOCOL_SLASH,
		STATUS_PROTOCOL_VERSION_MAJOR,
		STATUS_PROTOCOL_VERSION_MINOR,
		STATUS_CODE_BEGIN,
		STATUS_CODE,
		STATUS_MESSAGE_BEGIN,
		STATUS_MESSAGE,
		STATUS_MESSAGE_LF,

		// message-headers
		HEADER_NAME_BEGIN = 200,
		HEADER_NAME,
		HEADER_VALUE,
		HEADER_END_LF,

		LWS_BEGIN = 300,
		LWS_LF,
		LWS_SP_HT_BEGIN,
		LWS_SP_HT,

		// message-content
		CONTENT_BEGIN = 400,
		CONTENT,
		CONTENT_CHUNK_SIZE_BEGIN = 410,
		CONTENT_CHUNK_SIZE,
		CONTENT_CHUNK_LF1,
		CONTENT_CHUNK_BODY,
		CONTENT_CHUNK_LF2,
		CONTENT_CHUNK_CR3,
		CONTENT_CHUNK_LF3
	}; // }}}

public:
	virtual void messageBegin(BufferRef&& method, BufferRef&& entity, int version_major, int version_minor);
	virtual void messageBegin(int version_major, int version_minor, int code, BufferRef&& text);
	virtual void messageBegin();

	virtual void messageHeader(BufferRef&& name, BufferRef&& value);
	virtual bool messageHeaderEnd();
	virtual bool messageContent(BufferRef&& chunk);
	virtual bool messageEnd();

public:
	explicit HttpMessageProcessor(ParseMode mode = MESSAGE);

	State state() const;
	const char *state_str() const;

	HttpMessageError process(BufferRef&& chunk, std::size_t& nparsed);

	ssize_t contentLength() const;

private:
	bool passContent(BufferRef&& chunk, std::size_t& nparsed, std::size_t& ofp);

	static inline bool isChar(char value);
	static inline bool isControl(char value);
	static inline bool isSeparator(char value);
	static inline bool isToken(char value);
	static inline bool isText(char value);

private:
	enum { // lexer constants
		CR = 0x0D,
		LF = 0x0A,
		SP = 0x20,
		HT = 0x09
	};

	ParseMode mode_;
	enum State state_;

	// request-line
	BufferRef method_;
	BufferRef entity_;
	int versionMajor_;
	int versionMinor_;

	// status-line
	int code_;
	BufferRef message_;

	// current parsed header
	BufferRef name_;
	BufferRef value_;

	// body
	bool chunked_;			//!< whether or not request content is chunked encoded
	ssize_t contentLength_;	//!< content length of whole content or current chunk
	ChainFilter filters_;	//!< filters to apply to the message body before forwarding to the callback.
};

} // namespace x0

//@}

// {{{ inlines
namespace x0 {

inline enum HttpMessageProcessor::State HttpMessageProcessor::state() const
{
	return state_;
}

inline ssize_t HttpMessageProcessor::contentLength() const
{
	return contentLength_;
}

} // namespace x0

#undef TRACE

// }}}

#endif
