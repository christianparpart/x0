#ifndef sw_x0_message_parser_hpp
#define sw_x0_message_parser_hpp (1)

#include <x0/buffer.hpp>
#include <x0/buffer_ref.hpp>
#include <x0/io/chain_filter.hpp>
#include <x0/io/chunked_decoder.hpp>
#include <x0/defines.hpp>
#include <x0/api.hpp>

#include <system_error>
#include <functional>
#include <cctype>

namespace x0 {

enum class message_parser_error // {{{
{
	invalid_syntax,
}; // }}}

/**
 * \class message_parser
 * \brief implements HTTP/1.1 messages (request and response)
 */
class message_parser
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
		MESSAGE_END,

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
	}; // }}}

	enum {
		CR = 0x0D,
		LF = 0x0A,
		SP = 0x20,
		HT = 0x09,
	};

public:
	std::function<void(buffer_ref&&, buffer_ref&&, int, int)> on_request;
	std::function<void(int, int, int, buffer_ref&&)> on_response;
	std::function<void()> on_message;

	std::function<void(buffer_ref&&, buffer_ref&&)> on_header;
	std::function<void()> on_header_done;
	std::function<void(buffer_ref&&)> on_content;

	std::function<bool()> on_complete;

public:
	explicit message_parser(mode_type mode = MESSAGE);

	message_parser::state state() const;
	void reset(enum message_parser::state s = MESSAGE_BEGIN);
	std::size_t parse(buffer_ref&& chunk);
	std::size_t parse(buffer_ref&& chunk, std::error_code& ec);
	void abort();

private:
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
	mode_type mode_;
	enum state state_;
	bool abort_;

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
	chunked_decoder chunked_decoder_;
	chain_filter filter_chain_;
};

} // namespace x0

// {{{ inlines
#if 1
#	define TRACE(msg...)
#else
#	define TRACE(msg...) DEBUG("message_parser: " msg)
#endif

namespace x0 {

inline message_parser::message_parser(mode_type mode) :
	mode_(mode),
	state_(MESSAGE_BEGIN),
	abort_(),
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
	chunked_decoder_(),
	filter_chain_()
{
}

inline enum message_parser::state message_parser::state() const
{
	return state_;
}

inline void message_parser::reset(enum message_parser::state s)
{
	state_ = s;

	method_.clear();
	entity_.clear();
	version_major_ = 0;
	version_minor_ = 0;
	code_ = 0;
	message_.clear();

	name_.clear();
	value_.clear();

	content_chunked_ = false;
	content_length_ = -1;
	chunked_decoder_.reset();
	filter_chain_.clear();
}

inline const char *state2str(enum message_parser::state s)
{
	switch (s) {
		// artificial
		case message_parser::SYNTAX_ERROR: return "syntax-error";
		case message_parser::MESSAGE_BEGIN: return "message-begin";
		case message_parser::MESSAGE_END: return "message-end";

		// request-line
		case message_parser::REQUEST_LINE_BEGIN: return "request-line-begin";
		case message_parser::REQUEST_METHOD: return "request-method";
		case message_parser::REQUEST_ENTITY_BEGIN: return "request-entity-begin";
		case message_parser::REQUEST_ENTITY: return "request-entity";
		case message_parser::REQUEST_PROTOCOL_BEGIN: return "request-protocol-begin";
		case message_parser::REQUEST_PROTOCOL_T1: return "request-protocol-t1";
		case message_parser::REQUEST_PROTOCOL_T2: return "request-protocol-t2";
		case message_parser::REQUEST_PROTOCOL_P: return "request-protocol-p";
		case message_parser::REQUEST_PROTOCOL_SLASH: return "request-protocol-slash";
		case message_parser::REQUEST_PROTOCOL_VERSION_MAJOR: return "request-protocol-version-major";
		case message_parser::REQUEST_PROTOCOL_VERSION_MINOR: return "request-protocol-version-minor";
		case message_parser::REQUEST_LINE_LF: return "request-line-lf";

		// Status-Line
		case message_parser::STATUS_LINE_BEGIN: return "status-line-begin";
		case message_parser::STATUS_PROTOCOL_BEGIN: return "status-protocol-begin";
		case message_parser::STATUS_PROTOCOL_T1: return "status-protocol-t1";
		case message_parser::STATUS_PROTOCOL_T2: return "status-protocol-t2";
		case message_parser::STATUS_PROTOCOL_P: return "status-protocol-t2";
		case message_parser::STATUS_PROTOCOL_SLASH: return "status-protocol-t2";
		case message_parser::STATUS_PROTOCOL_VERSION_MAJOR: return "status-protocol-version-major";
		case message_parser::STATUS_PROTOCOL_VERSION_MINOR: return "status-protocol-version-minor";
		case message_parser::STATUS_CODE_BEGIN: return "status-code-begin";
		case message_parser::STATUS_CODE: return "status-code";
		case message_parser::STATUS_MESSAGE_BEGIN: return "status-message-begin";
		case message_parser::STATUS_MESSAGE: return "status-message";
		case message_parser::STATUS_MESSAGE_LF: return "status-message-lf";

		// message header
		case message_parser::HEADER_NAME_BEGIN: return "header-name-begin";
		case message_parser::HEADER_NAME: return "header-name";
		case message_parser::HEADER_VALUE: return "header-value";
		case message_parser::HEADER_END_LF: return "header-end-lf";

		// LWS
		case message_parser::LWS_BEGIN: return "lws-begin";
		case message_parser::LWS_LF: return "lws-lf";
		case message_parser::LWS_SP_HT_BEGIN: return "lws-sp-ht-begin";
		case message_parser::LWS_SP_HT: return "lws-sp-ht";

		// message content
		case message_parser::CONTENT_BEGIN: return "content-begin";
		case message_parser::CONTENT: return "content";
	}
	return "UNKNOWN";
}

inline std::size_t message_parser::parse(buffer_ref&& chunk)
{
	std::error_code ignored;
	return parse(std::move(chunk), ignored);
}

inline std::size_t message_parser::parse(buffer_ref&& chunk, std::error_code& ec)
{
	const char *i = chunk.begin();
	const char *e = chunk.end();
	std::size_t offset = 0;

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

	TRACE("parse: size: %ld", chunk.size());

	abort_ = false;

	if (state_ == CONTENT)
	{
		if (!pass_content(std::move(chunk), ec, offset))
			return offset;

		i += offset;
	}

	while (!abort_ && i != e)
	{
#if 1
		if (std::isprint(*i))
			TRACE("parse: %4ld, 0x%02X (%c),  %s", offset, *i, *i, state2str(state_));
		else                                            
			TRACE("parse: %4ld, 0x%02X,     %s", offset, *i, state2str(state_));
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
						if (on_message)
							on_message();

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
						state_ = MESSAGE_END;

					if (on_header_done)
						on_header_done();

					if (!content_expected && on_complete)
						if (!on_complete())
							return offset;

					++offset;
					++i;
				} else
					state_ = SYNTAX_ERROR;
				break;
			case CONTENT_BEGIN:
				if (content_length_ <= 0 && !content_chunked_) {
					state_ = SYNTAX_ERROR;
					break;
				}
				state_ = CONTENT;
			case CONTENT:
			{
				std::size_t nparsed = 0;
				if (!pass_content(chunk.ref(offset), ec, nparsed))
					return offset + nparsed;

				offset += nparsed;
				i += nparsed;
				break;
			}
			case MESSAGE_END:
				return offset;
			case SYNTAX_ERROR:
			{
				ec = std::make_error_code(std::errc::invalid_argument);
				//! \todo ec = make_error_code(message_parser_error::invalid_syntax);

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

			if (on_complete)
				on_complete();

			// subsequent calls to parse() process possible next requests then.
			state_ = MESSAGE_BEGIN;
		}
	}

	return offset;
}

inline void message_parser::abort()
{
	abort_ = true;
}

inline void message_parser::pass_request()
{
	TRACE("request-line: method=%s, entity=%s, vmaj=%d, vmin=%d",
			method_.str().c_str(), entity_.str().c_str(), version_major_, version_minor_);

	if (on_request)
		on_request(std::move(method_), std::move(entity_), version_major_, version_minor_);
}

inline void message_parser::pass_response()
{
	TRACE("status-line: HTTP/%d.%d, code=%d, message=%s", version_major_, version_minor_, code_, message_.str().c_str());

	if (on_response)
		on_response(version_major_, version_minor_, code_, std::move(message_));
}

inline void message_parser::pass_header()
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

	if (on_header)
		on_header(std::move(name_), std::move(value_));

	name_.clear();
	value_.clear();
}

inline bool message_parser::pass_content(buffer_ref&& chunk, std::error_code& ec, std::size_t& nparsed)
{
	if (content_chunked_)
	{
		buffer result(chunked_decoder_.process(chunk));
		nparsed += chunk.size();

		if (chunked_decoder_.state() == chunked_decoder::END)
			state_ = MESSAGE_END;

		if (!filter_chain_.empty())
			result = filter_chain_.process(result);

		if (!result.empty() && on_content)
			on_content(result);

		if (state_ == MESSAGE_END && on_complete)
		{
			state_ = MESSAGE_BEGIN;
			chunked_decoder_.reset();
			return on_complete();
		}
	}
	else if (content_length_ > 0)
	{
		// shrink down to remaining content-length
		buffer_ref c(chunk);
		if (chunk.size() > static_cast<std::size_t>(content_length_))
			c.shr(-(c.size() - content_length_));

		nparsed += c.size();
		content_length_ -= c.size();

		if (on_content)
			on_content(filter_chain_.process(c));

		if (content_length_ == 0)
		{
			state_ = MESSAGE_END;

			if (on_complete)
			{
				TRACE("content fully parsed -> complete");
				state_ = MESSAGE_BEGIN;
				return on_complete();
			}
		}
	}
	else
	{
		nparsed += chunk.size();

		if (on_content)
			on_content(filter_chain_.process(chunk));
	}

	return true;
}

inline bool message_parser::is_char(char value)
{
	return value >= 0 && value <= 127;
}

inline bool message_parser::is_ctl(char value)
{
	return (value >= 0 && value <= 31) || value == 127;
}

inline bool message_parser::is_seperator(char value)
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

inline bool message_parser::is_token(char value)
{
	return is_char(value) && !(is_ctl(value) || is_seperator(value));
}

inline bool message_parser::is_text(char value)
{
	// TEXT = <any OCTET except CTLs but including LWS>
	return !is_ctl(value) || value == SP || value == HT;
}

} // namespace x0

#undef TRACE

// }}}

#endif
