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
		HEADER_COLON,
		HEADER_VALUE_BEGIN,
		HEADER_VALUE,
		HEADER_VALUE_LF,
		HEADER_VALUE_END,
		HEADER_END_LF,

		// LWS ::= [CR LF] 1*(SP | HT)
		LWS_BEGIN = 300,
		LWS_LF,
		LWS_SP_HT_BEGIN,
		LWS_SP_HT,

		// message-content
		CONTENT_BEGIN = 400,
		CONTENT,
		CONTENT_ENDLESS = 405,
		CONTENT_CHUNK_SIZE_BEGIN = 410,
		CONTENT_CHUNK_SIZE,
		CONTENT_CHUNK_LF1,
		CONTENT_CHUNK_BODY,
		CONTENT_CHUNK_LF2,
		CONTENT_CHUNK_CR3,
		CONTENT_CHUNK_LF3
	}; // }}}

public:
	virtual bool onMessageBegin(const BufferRef& method, const BufferRef& entity, int versionMajor, int versionMinor);
	virtual bool onMessageBegin(int versionMajor, int versionMinor, int code, const BufferRef& text);
	virtual bool onMessageBegin();
	virtual bool onMessageHeader(const BufferRef& name, const BufferRef& value);
	virtual bool onMessageHeaderEnd();
	virtual bool onMessageContent(const BufferRef& chunk);
	virtual bool onMessageEnd();

	bool isProcessingHeader() const;
	bool isProcessingBody() const;

public:
	explicit HttpMessageProcessor(ParseMode mode);

	State state() const;
	const char *state_str() const;

	size_t process(const BufferRef& chunk, size_t* nparsed = nullptr);

	ssize_t contentLength() const;

private:
	static inline bool isChar(char value);
	static inline bool isControl(char value);
	static inline bool isSeparator(char value);
	static inline bool isToken(char value);
	static inline bool isText(char value);

private:
	// lexer constants
	enum {
		CR = 0x0D,
		LF = 0x0A,
		SP = 0x20,
		HT = 0x09
	};

	ParseMode mode_;
	State state_;       //!< the current parser/processing state

	// implicit LWS handling
	State lwsNext_;     //!< state to apply on successfull LWS
	State lwsNull_;     //!< state to apply on (CR LF) but no 1*(SP | HT)

	// request-line
	BufferRef method_;		//!< HTTP request method
	BufferRef entity_;		//!< HTTP request entity

	int versionMajor_;		//!< HTTP request/response version major
	int versionMinor_;		//!< HTTP request/response version minor

	// status-line
	int code_;				//!< response status code
	BufferRef message_;		//!< response status message

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

inline bool HttpMessageProcessor::isProcessingHeader() const
{
	// XXX should we include request-line and status-line here, too?
	switch (state_) {
		case HEADER_NAME_BEGIN:
		case HEADER_NAME:
		case HEADER_COLON:
		case HEADER_VALUE_BEGIN:
		case HEADER_VALUE:
		case HEADER_VALUE_LF:
		case HEADER_VALUE_END:
		case HEADER_END_LF:
			return true;
		default:
			return false;
	}
}

inline bool HttpMessageProcessor::isProcessingBody() const
{
	switch (state_) {
		case CONTENT_BEGIN:
		case CONTENT:
		case CONTENT_ENDLESS:
		case CONTENT_CHUNK_SIZE_BEGIN:
		case CONTENT_CHUNK_SIZE:
		case CONTENT_CHUNK_LF1:
		case CONTENT_CHUNK_BODY:
		case CONTENT_CHUNK_LF2:
		case CONTENT_CHUNK_CR3:
		case CONTENT_CHUNK_LF3:
			return true;
		default:
			return false;
	}
}

} // namespace x0

#undef TRACE

// }}}

#endif
