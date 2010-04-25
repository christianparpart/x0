#ifndef sw_x0_message_processor_hpp
#define sw_x0_message_processor_hpp (1)

#include <x0/buffer.hpp>
#include <x0/buffer_ref.hpp>
#include <x0/io/chain_filter.hpp>
#include <x0/defines.hpp>
#include <x0/api.hpp>

#include <system_error>
#include <functional>
#include <cctype>

// {{{ enum class http_message_error
namespace x0 {
	enum class http_message_error
	{
		success = 0,
		partial,
		aborted,
		invalid_syntax,
	};

	const std::error_category& http_message_category() throw();

	std::error_code make_error_code(http_message_error ec);
	std::error_condition make_error_condition(http_message_error ec);
}

namespace std {
	// implicit conversion from http_message_error to error_code
	template<> struct is_error_code_enum<x0::http_message_error> : public true_type {};
}

// {{{ inlines
namespace x0 {
	inline std::error_code make_error_code(http_message_error ec)
	{
		return std::error_code(static_cast<int>(ec), x0::http_message_category());
	}

	inline std::error_condition make_error_condition(http_message_error ec)
	{
		return std::error_condition(static_cast<int>(ec), x0::http_message_category());
	}
}
// }}}
// }}}

namespace x0 {

/**
 * \class message_processor
 * \brief implements an HTTP/1.1 (request/response) message parser and processor
 */
class message_processor
{
public:
	enum mode_type { // {{{
		REQUEST,
		RESPONSE,
		MESSAGE
	}; // }}}

	enum state { // {{{
		// artificial
		SYNTAX_ERROR = 0,
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
	virtual void message_begin(buffer_ref&& method, buffer_ref&& entity, int version_major, int version_minor);
	virtual void message_begin(int version_major, int version_minor, int code, buffer_ref&& text);
	virtual void message_begin();

	virtual void message_header(buffer_ref&& name, buffer_ref&& value);
	virtual bool message_header_done();
	virtual bool message_content(buffer_ref&& chunk);
	virtual bool message_end();

public:
	explicit message_processor(mode_type mode = MESSAGE);

	message_processor::state state() const;
	const char *state_str() const;

	std::size_t process(buffer_ref&& chunk, std::error_code& ec);
	std::size_t next_offset() const;

	void clear();

private:
	void reset();
	void pass_request();
	void pass_response();
	void pass_header();
	bool pass_content(buffer_ref&& chunk, std::error_code& ec, std::size_t& nparsed);

	static bool is_char(char value);
	static bool is_ctl(char value);
	static bool is_seperator(char value);
	static bool is_token(char value);
	static bool is_text(char value);

private:
	enum { // lexer constants
		CR = 0x0D,
		LF = 0x0A,
		SP = 0x20,
		HT = 0x09,
	};

	mode_type mode_;
	enum state state_;
	std::size_t next_offset_;

	// request-line
	buffer_ref method_;
	buffer_ref entity_;
	int version_major_;
	int version_minor_;

	// status-line
	int code_;
	buffer_ref message_;

	// current parsed header
	buffer_ref name_;
	buffer_ref value_;

	// body
	bool content_chunked_;            //!< whether or not request content is chunked encoded
	ssize_t content_length_;          //!< content length of whole content or current chunk
	chain_filter filter_chain_;
};

} // namespace x0

// {{{ inlines
#if 0
#	define TRACE(msg...)
#else
#	define TRACE(msg...) DEBUG("message_processor: " msg)
#endif

namespace x0 {

/** initializes the HTTP/1.1 message processor.
 *
 * \param mode REQUEST: parses and processes an HTTP/1.1 Request,
 *             RESPONSE: parses and processes an HTTP/1.1 Response.
 *             MESSAGE: parses and processes an HTTP/1.1 message, that is, without the first request/status line - just headers and content.
 *
 * \note No member variable may be modified after the hook invokation returned with
 *       a false return code, which means, that processing is to be cancelled
 *       and thus, may imply, that the object itself may have been already deleted.
 */
inline message_processor::message_processor(mode_type mode) :
	mode_(mode),
	state_(MESSAGE_BEGIN),
	next_offset_(0),
	method_(),
	entity_(),
	version_major_(0),
	version_minor_(0),
	code_(0),
	message_(),
	name_(),
	value_(),
	content_chunked_(false),
	content_length_(-1),
	filter_chain_()
{
}

inline enum message_processor::state message_processor::state() const
{
	return state_;
}

inline const char *message_processor::state_str() const
{
	switch (state_) {
		// artificial
		case message_processor::SYNTAX_ERROR: return "syntax-error";
		case message_processor::MESSAGE_BEGIN: return "message-begin";

		// request-line
		case message_processor::REQUEST_LINE_BEGIN: return "request-line-begin";
		case message_processor::REQUEST_METHOD: return "request-method";
		case message_processor::REQUEST_ENTITY_BEGIN: return "request-entity-begin";
		case message_processor::REQUEST_ENTITY: return "request-entity";
		case message_processor::REQUEST_PROTOCOL_BEGIN: return "request-protocol-begin";
		case message_processor::REQUEST_PROTOCOL_T1: return "request-protocol-t1";
		case message_processor::REQUEST_PROTOCOL_T2: return "request-protocol-t2";
		case message_processor::REQUEST_PROTOCOL_P: return "request-protocol-p";
		case message_processor::REQUEST_PROTOCOL_SLASH: return "request-protocol-slash";
		case message_processor::REQUEST_PROTOCOL_VERSION_MAJOR: return "request-protocol-version-major";
		case message_processor::REQUEST_PROTOCOL_VERSION_MINOR: return "request-protocol-version-minor";
		case message_processor::REQUEST_LINE_LF: return "request-line-lf";

		// Status-Line
		case message_processor::STATUS_LINE_BEGIN: return "status-line-begin";
		case message_processor::STATUS_PROTOCOL_BEGIN: return "status-protocol-begin";
		case message_processor::STATUS_PROTOCOL_T1: return "status-protocol-t1";
		case message_processor::STATUS_PROTOCOL_T2: return "status-protocol-t2";
		case message_processor::STATUS_PROTOCOL_P: return "status-protocol-t2";
		case message_processor::STATUS_PROTOCOL_SLASH: return "status-protocol-t2";
		case message_processor::STATUS_PROTOCOL_VERSION_MAJOR: return "status-protocol-version-major";
		case message_processor::STATUS_PROTOCOL_VERSION_MINOR: return "status-protocol-version-minor";
		case message_processor::STATUS_CODE_BEGIN: return "status-code-begin";
		case message_processor::STATUS_CODE: return "status-code";
		case message_processor::STATUS_MESSAGE_BEGIN: return "status-message-begin";
		case message_processor::STATUS_MESSAGE: return "status-message";
		case message_processor::STATUS_MESSAGE_LF: return "status-message-lf";

		// message header
		case message_processor::HEADER_NAME_BEGIN: return "header-name-begin";
		case message_processor::HEADER_NAME: return "header-name";
		case message_processor::HEADER_VALUE: return "header-value";
		case message_processor::HEADER_END_LF: return "header-end-lf";

		// LWS
		case message_processor::LWS_BEGIN: return "lws-begin";
		case message_processor::LWS_LF: return "lws-lf";
		case message_processor::LWS_SP_HT_BEGIN: return "lws-sp-ht-begin";
		case message_processor::LWS_SP_HT: return "lws-sp-ht";

		// message content
		case message_processor::CONTENT_BEGIN: return "content-begin";
		case message_processor::CONTENT: return "content";
		case message_processor::CONTENT_CHUNK_SIZE_BEGIN: return "content-chunk-size-begin";
		case message_processor::CONTENT_CHUNK_SIZE: return "content-chunk-size";
		case message_processor::CONTENT_CHUNK_LF1: return "content-chunk-lf1";
		case message_processor::CONTENT_CHUNK_BODY: return "content-chunk-body";
		case message_processor::CONTENT_CHUNK_LF2: return "content-chunk-lf2";
		case message_processor::CONTENT_CHUNK_CR3: return "content-chunk-cr3";
		case message_processor::CONTENT_CHUNK_LF3: return "content-chunk_lf3";
	}
	return "UNKNOWN";
}

/** processes a message-chunk.
 *
 * \param chunk the chunk of bytes to process
 * \param ec    the error code describing the possible error
 *
 * \return      the number of bytes actually processed.
 */
inline std::size_t message_processor::process(buffer_ref&& chunk, std::error_code& ec)
{
	/*
	 * CR               = 0x0D
	 * LF               = 0x0A
	 * SP               = 0x20
	 * HT               = 0x09
	 *
	 * CRLF             = CR LF
	 * LWS              = [CRLF] 1*( SP | HT )
	 *
	 * HTTP-message     = Request | Response
	 *
	 * generic-message  = start-line
	 *                    *(message-header CRLF)
	 *                    CRLF
	 *                    [ message-body ]
	 *                 
	 * start-line       = Request-Line | Status-Line
	 *
	 * Request-Line     = Method SP Request-URI SP HTTP-Version CRLF
	 *
	 * Method           = "OPTIONS" | "GET" | "HEAD"
	 *                  | "POST"    | "PUT" | "DELETE"
	 *                  | "TRACE"   | "CONNECT"
	 *                  | extension-method
	 *
	 * Request-URI      = "*" | absoluteURI | abs_path | authority
	 *
	 * extension-method = token
	 *
	 * Status-Line      = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
	 *
	 * HTTP-Version     = "HTTP" "/" 1*DIGIT "." 1*DIGIT
	 * Status-Code      = 3*DIGIT
	 * Reason-Phrase    = *<TEXT, excluding CR, LF>
	 *
	 * absoluteURI      = "http://" [user ':' pass '@'] hostname [abs_path] [qury]
	 * abs_path         = ...
	 * authority        = ...
	 * token            = ...
	 *
	 * message-header   = field-name ":" [ field-value ]
	 * field-name       = token
	 * field-value      = *( field-content | LWS )
	 * field-content    = <the OCTETs making up the field-value
	 *                    and consisting of either *TEXT or combinations
	 *                    of token, separators, and quoted-string>
	 *
	 * message-body     = entity-body
	 *                  | <entity-body encoded as per Transfer-Encoding>
	 */

	const char *i = chunk.begin();
	const char *e = chunk.end();
	std::size_t offset = 0;

	ec.clear();

	TRACE("parse: size: %ld", chunk.size());

	if (state_ == CONTENT)
	{
		if (!pass_content(std::move(chunk), ec, offset))
			return offset;

		i += offset;
	}

	while (i != e)
	{
#if 1
		if (std::isprint(*i))
			TRACE("parse: %4ld, 0x%02X (%c),  %s", offset, *i, *i, state_str());
		else                                            
			TRACE("parse: %4ld, 0x%02X,     %s", offset, *i, state_str());
#endif

		switch (state_)
		{
			case MESSAGE_BEGIN:
				switch (mode_) {
					case REQUEST:
						state_ = REQUEST_LINE_BEGIN;
						break;
					case RESPONSE:
						state_ = STATUS_LINE_BEGIN;
						break;
					case MESSAGE:
						state_ = HEADER_NAME_BEGIN;

						// an internet message has no special top-line,
						// so we just invoke the callback right away
						message_begin();

						break;
				}
				break;
			case REQUEST_LINE_BEGIN:
				if (is_token(*i))
				{
					state_ = REQUEST_METHOD;
					method_ = chunk.ref(offset, 1);

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case REQUEST_METHOD:
				if (*i == SP)
				{
					state_ = REQUEST_ENTITY_BEGIN;
					++offset;
					++i;
				}
				else if (!is_token(*i))
					state_ = SYNTAX_ERROR;
				else
				{
					method_.shr();
					++offset;
					++i;
				}
				break;
			case REQUEST_ENTITY_BEGIN:
				if (std::isprint(*i))
				{
					entity_ = chunk.ref(offset, 1);
					state_ = REQUEST_ENTITY;

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case REQUEST_ENTITY:
				if (*i == SP)
				{
					state_ = REQUEST_PROTOCOL_BEGIN;
					++offset;
					++i;
				}
				else if (std::isprint(*i))
				{
					entity_.shr();
					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case REQUEST_PROTOCOL_BEGIN:
				if (*i != 'H')
					state_ = SYNTAX_ERROR;
				else
				{
					state_ = REQUEST_PROTOCOL_T1;
					++offset;
					++i;
				}
				break;
			case REQUEST_PROTOCOL_T1:
				if (*i != 'T')
					state_ = SYNTAX_ERROR;
				else
				{
					state_ = REQUEST_PROTOCOL_T2;
					++offset;
					++i;
				}
				break;
			case REQUEST_PROTOCOL_T2:
				if (*i != 'T')
					state_ = SYNTAX_ERROR;
				else
				{
					state_ = REQUEST_PROTOCOL_P;
					++offset;
					++i;
				}
				break;
			case REQUEST_PROTOCOL_P:
				if (*i != 'P')
					state_ = SYNTAX_ERROR;
				else
				{
					state_ = REQUEST_PROTOCOL_SLASH;
					++offset;
					++i;
				}
				break;
			case REQUEST_PROTOCOL_SLASH:
				if (*i != '/')
					state_ = SYNTAX_ERROR;
				else
				{
					state_ = REQUEST_PROTOCOL_VERSION_MAJOR;
					++offset;
					++i;
				}
				break;
			case REQUEST_PROTOCOL_VERSION_MAJOR:
				if (*i == '.')
				{
					state_ = REQUEST_PROTOCOL_VERSION_MINOR;
					++offset;
					++i;
				}
				else if (!std::isdigit(*i))
					state_ = SYNTAX_ERROR;
				else
				{
					version_major_ = version_major_ * 10 + *i - '0';
					++offset;
					++i;
				}
				break;
			case REQUEST_PROTOCOL_VERSION_MINOR:
				if (*i == CR)
				{
					state_ = REQUEST_LINE_LF;
					++offset;
					++i;
				}
				else if (!std::isdigit(*i))
					state_ = SYNTAX_ERROR;
				else
				{
					version_minor_ = version_minor_ * 10 + *i - '0';
					++offset;
					++i;
				}
				break;
			case REQUEST_LINE_LF:
				if (*i == LF)
				{
					pass_request();
					state_ = HEADER_NAME_BEGIN;

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case STATUS_LINE_BEGIN:
			case STATUS_PROTOCOL_BEGIN:
				if (*i != 'H')
					state_ = SYNTAX_ERROR;
				else
				{
					state_ = STATUS_PROTOCOL_T1;
					++offset;
					++i;
				}
				break;
			case STATUS_PROTOCOL_T1:
				if (*i != 'T')
					state_ = SYNTAX_ERROR;
				else
				{
					state_ = STATUS_PROTOCOL_T2;
					++offset;
					++i;
				}
				break;
			case STATUS_PROTOCOL_T2:
				if (*i != 'T')
					state_ = SYNTAX_ERROR;
				else
				{
					state_ = STATUS_PROTOCOL_P;
					++offset;
					++i;
				}
				break;
			case STATUS_PROTOCOL_P:
				if (*i != 'P')
					state_ = SYNTAX_ERROR;
				else
				{
					state_ = STATUS_PROTOCOL_SLASH;
					++offset;
					++i;
				}
				break;
			case STATUS_PROTOCOL_SLASH:
				if (*i != '/')
					state_ = SYNTAX_ERROR;
				else
				{
					state_ = STATUS_PROTOCOL_VERSION_MAJOR;
					++offset;
					++i;
				}
				break;
			case STATUS_PROTOCOL_VERSION_MAJOR:
				if (*i == '.')
				{
					state_ = STATUS_PROTOCOL_VERSION_MINOR;
					++offset;
					++i;
				}
				else if (!std::isdigit(*i))
					state_ = SYNTAX_ERROR;
				else
				{
					version_major_ = version_major_ * 10 + *i - '0';
					++offset;
					++i;
				}
				break;
			case STATUS_PROTOCOL_VERSION_MINOR:
				if (*i == SP)
				{
					state_ = STATUS_CODE_BEGIN;
					++offset;
					++i;
				}
				else if (!std::isdigit(*i))
					state_ = SYNTAX_ERROR;
				else
				{
					version_minor_ = version_minor_ * 10 + *i - '0';
					++offset;
					++i;
				}
				break;
			case STATUS_CODE_BEGIN:
				if (!std::isdigit(*i))
				{
					code_ = SYNTAX_ERROR;
					break;
				}
				state_ = STATUS_CODE;
				/* fall through */
			case STATUS_CODE:
				if (std::isdigit(*i))
				{
					code_ = code_ * 10 + *i - '0';
					++offset;
					++i;
				}
				else if (*i == SP)
				{
					state_ = STATUS_MESSAGE_BEGIN;
					++offset;
					++i;
				}
				else if (*i == CR) // no Status-Message passed
				{
					state_ = STATUS_MESSAGE_LF;
					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case STATUS_MESSAGE_BEGIN:
				if (is_text(*i))
				{
					state_ = STATUS_MESSAGE;
					message_ = chunk.ref(offset, 1);
					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case STATUS_MESSAGE:
				if (is_text(*i) && *i != CR && *i != LF)
				{
					message_.shr();
					++offset;
					++i;
				}
				else if (*i == CR)
				{
					state_ = STATUS_MESSAGE_LF;
					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case STATUS_MESSAGE_LF:
				if (*i == LF)
				{
					pass_response();
					state_ = HEADER_NAME_BEGIN;
					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case HEADER_NAME_BEGIN:
				if (is_token(*i)) {
					state_ = HEADER_NAME;
					name_ = chunk.ref(offset, 1);

					++offset;
					++i;
				}
				else if (*i == CR)
				{
					state_ = HEADER_END_LF;

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case HEADER_NAME:
				if (*i == ':')
				{
					state_ = LWS_BEGIN;

					++offset;
					++i;
				}
				else if (is_token(*i))
				{
					name_.shr();

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case LWS_BEGIN:
				if (*i == CR)
				{
					state_ = LWS_LF;

					++offset;
					++i;
				}
				else if (*i == SP || *i == HT)
				{
					state_ = LWS_SP_HT;

					++offset;
					++i;
				}
				else if (std::isprint(*i))
				{
					if (value_.empty())
						value_ = chunk.ref(offset, 1);

					state_ = HEADER_VALUE;

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case LWS_LF:
				if (*i == LF)
				{
					state_ = LWS_SP_HT_BEGIN;

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case LWS_SP_HT_BEGIN:
				if (*i == SP || *i == HT)
				{
					if (!value_.empty())
						value_.shr(3); // CR LF (SP | HT)

					state_ = LWS_SP_HT;

					++offset;
					++i;
				}
				else
				{
					state_ = HEADER_NAME_BEGIN; // XXX no offset/i-update

					pass_header();
				}
				break;
			case LWS_SP_HT:
				if (*i == SP || *i == HT)
				{
					if (!value_.empty())
						value_.shr();

					++offset;
					++i;
				}
				else if (std::isprint(*i))
				{
					state_ = HEADER_VALUE;

					if (value_.empty())
						value_ = chunk.ref(offset, 1);
					else
						value_.shr();

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case HEADER_VALUE:
				if (*i == CR)
				{
					state_ = LWS_LF;

					++offset;
					++i;
				}
				else if (std::isprint(*i))
				{
					value_.shr();

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case HEADER_END_LF:
				if (*i == LF)
				{
					bool content_expected = content_length_ > 0 || content_chunked_;

					if (content_expected)
						state_ = CONTENT_BEGIN;
					else
						reset();

					next_offset_ = ++offset;
					++i;

					if (!message_header_done())
					{
						ec = make_error_code(http_message_error::aborted);
						return offset;
					}

					if (!content_expected)
					{
						if (!message_end())
						{
							ec = make_error_code(http_message_error::aborted);
							return offset;
						}
					}
				} else
					state_ = SYNTAX_ERROR;
				break;
			case CONTENT_BEGIN:
				if (content_chunked_)
					state_ = CONTENT_CHUNK_SIZE_BEGIN;
				else if (content_length_ < 0)
					state_ = SYNTAX_ERROR;
				else
					state_ = CONTENT;
				break;
			case CONTENT:
			{
				std::size_t nparsed = 0;

				if (!pass_content(chunk.ref(offset), ec, nparsed))
				{
					ec = make_error_code(http_message_error::aborted);
					return offset + nparsed;
				}

				offset += nparsed;
				i += nparsed;
				break;
			}
			case CONTENT_CHUNK_SIZE_BEGIN:
				if (!std::isxdigit(*i))
				{
					state_ = SYNTAX_ERROR;
					break;
				}
				state_ = CONTENT_CHUNK_SIZE;
				content_length_ = 0;
				/* fall through */
			case CONTENT_CHUNK_SIZE:
				if (*i == CR)
				{
					state_ = CONTENT_CHUNK_LF1;
					++offset;
					++i;
				}
				else if (*i >= '0' && *i <= '9')
				{
					content_length_ = content_length_ * 16 + *i - '0';
					++offset;
					++i;
				}
				else if (*i >= 'a' && *i <= 'f')
				{
					content_length_ = content_length_ * 16 + 10 + *i - 'a';
					++offset;
					++i;
				}
				else if (*i >= 'A' && *i <= 'F')
				{
					content_length_ = content_length_ * 16 + 10 + *i - 'A';
					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case CONTENT_CHUNK_LF1:
				if (*i != LF)
					state_ = SYNTAX_ERROR;
				else
				{
					TRACE("content_length: %ld", content_length_);
					if (content_length_ != 0)
						state_ = CONTENT_CHUNK_BODY;
					else
						state_ = CONTENT_CHUNK_CR3;

					++offset;
					++i;
				}
				break;
			case CONTENT_CHUNK_BODY:
				if (content_length_)
				{
					std::size_t nparsed = 0;

					if (!pass_content(chunk.ref(offset), ec, nparsed))
					{
						ec = make_error_code(http_message_error::aborted);
						return offset + nparsed;
					}

					offset += nparsed;
					i += nparsed;
				}
				else if (*i == CR)
				{
					state_ = CONTENT_CHUNK_LF2;
					++offset;
					++i;
				}
				break;
			case CONTENT_CHUNK_LF2:
				if (*i != LF)
					state_ = SYNTAX_ERROR;
				else
				{
					state_ = CONTENT_CHUNK_SIZE;
					++offset;
					++i;
				}
				break;
			case CONTENT_CHUNK_CR3:
				if (*i != CR)
					state_ = SYNTAX_ERROR;
				else
				{
					state_ = CONTENT_CHUNK_LF3;
					++offset;
					++i;
				}
				break;
			case CONTENT_CHUNK_LF3:
				if (*i != LF)
					state_ = SYNTAX_ERROR;
				else
				{
					++offset;
					++i;

					if (!message_end())
						return offset;

					reset();
					next_offset_ = offset;
				}
				break;
			case SYNTAX_ERROR:
			{
				ec = make_error_code(http_message_error::invalid_syntax);

#if !defined(NDEBUG)
				if (std::isprint(*i))
					TRACE("parse: syntax error at offset: %ld, character: '%c'", offset, *i);
				else
					TRACE("parse: syntax error at offset: %ld, character: 0x%02X", offset, *i);

				return offset;
#else
				return offset;
#endif
			}
		}
	}
	// we've reached the end of the chunk

	if (state_ == CONTENT_BEGIN)
	{
		// we've just parsed all headers but no body yet.

		if (content_length_ < 0 && !content_chunked_)
		{
			// and there's no body to come

			if (!message_end())
				return offset;

			// subsequent calls to parse() process next request(s).
			reset();
			next_offset_ = offset;
		}
	}

	return offset;
}

inline std::size_t message_processor::next_offset() const
{
	return next_offset_;
}

inline void message_processor::pass_request()
{
	TRACE("request-line: method=%s, entity=%s, vmaj=%d, vmin=%d",
			method_.str().c_str(), entity_.str().c_str(), version_major_, version_minor_);

	message_begin(std::move(method_), std::move(entity_), version_major_, version_minor_);
}

inline void message_processor::pass_response()
{
	TRACE("status-line: HTTP/%d.%d, code=%d, message=%s", version_major_, version_minor_, code_, message_.str().c_str());

	message_begin(version_major_, version_minor_, code_, std::move(message_));
}

inline void message_processor::pass_header()
{
	TRACE("header: name='%s', value='%s'", name_.str().c_str(), value_.str().c_str());

	if (iequals(name_, "Content-Length"))
	{
		content_length_ = value_.as<int>();
	}
	else if (iequals(name_, "Transfer-Encoding"))
	{
		if (iequals(value_, "chunked"))
		{
			content_chunked_ = true;
		}
	}

	message_header(std::move(name_), std::move(value_));

	name_.clear();
	value_.clear();
}

inline bool message_processor::pass_content(buffer_ref&& chunk, std::error_code& ec, std::size_t& nparsed)
{
	if (content_length_ > 0)
	{
		// shrink down to remaining content-length
		buffer_ref c(chunk);
		if (chunk.size() > static_cast<std::size_t>(content_length_))
			c.shr(-(c.size() - content_length_));

		nparsed += c.size();
		content_length_ -= c.size();

		TRACE("pass_content: chunk_size=%ld, bytes_left=%ld; '%s'",
				c.size(), content_length_, c.str().c_str());

		if (content_chunked_)
		{
			buffer result(c);

			if (!filter_chain_.empty())
			{
				if (!message_content(filter_chain_.process(c)))
					return false;
			}
			else
			{
				if (!message_content(std::move(c)))
					return false;
			}

			if (state_ == MESSAGE_BEGIN)
			{
				return message_end();
			}
		}
		else // fixed-size content (via "Content-Length")
		{
			if (content_length_ == 0)
				reset();

			if (!message_content(filter_chain_.process(c)))
				return false;

			if (state_ == MESSAGE_BEGIN)
			{
				TRACE("content fully parsed -> complete");
				return message_end();
			}
		}
	}
	else if (content_length_ < 0)
	{
		nparsed += chunk.size();

		if (filter_chain_.empty())
		{
			if (!message_content(std::move(chunk)))
				return false;
		}
		else
		{
			if (!message_content(filter_chain_.process(chunk)))
				return false;
		}
	}

	return true;
}

inline void message_processor::clear()
{
	reset();
	next_offset_ = 0;
}

inline void message_processor::reset()
{
	TRACE("reset(next_offset=%ld): last_state=%s", next_offset_, state_str());

	version_major_ = 0;
	version_minor_ = 0;
	content_length_ = -1;
	state_ = MESSAGE_BEGIN;
}

inline bool message_processor::is_char(char value)
{
	return value >= 0 && value <= 127;
}

inline bool message_processor::is_ctl(char value)
{
	return (value >= 0 && value <= 31) || value == 127;
}

inline bool message_processor::is_seperator(char value)
{
	switch (value)
	{
		case '(':
		case ')':
		case '<':
		case '>':
		case '@':
		case ',':
		case ';':
		case ':':
		case '\\':
		case '"':
		case '/':
		case '[':
		case ']':
		case '?':
		case '=':
		case '{':
		case '}':
		case SP:
		case HT:
			return true;
		default:
			return false;
	}
}

inline bool message_processor::is_token(char value)
{
	return is_char(value) && !(is_ctl(value) || is_seperator(value));
}

inline bool message_processor::is_text(char value)
{
	// TEXT = <any OCTET except CTLs but including LWS>
	return !is_ctl(value) || value == SP || value == HT;
}

} // namespace x0

#undef TRACE

// }}}

#endif
