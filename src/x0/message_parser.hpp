#ifndef sw_x0_message_parser_hpp
#define sw_x0_message_parser_hpp (1)

#include <x0/buffer.hpp>
#include <x0/buffer_ref.hpp>
#include <x0/io/chain_filter.hpp>
#include <x0/io/chunked_decoder.hpp>
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
	enum state { // {{{
		// artificial
		SYNTAX_ERROR = 0,
		MESSAGE_BEGIN,
		MESSAGE_END,

		// Request-Line
		REQUEST_LINE_START = 100,
		METHOD,
		ENTITY_START,
		ENTITY,
		PROTOCOL_START,
		PROTOCOL,
		VERSION_MAJOR,
		VERSION_MINOR,
		REQUEST_LINE_LF,

		// Status-Line
		STATUS_LINE_START = 150,
		STATUS_PROTOCOL_START,
		STATUS_PROTOCOL,
		STATUS_CODE,
		STATUS_MESSAGE_START,
		STATUS_MESSAGE,

		// message-headers
		HEADER_NAME_START = 200,
		HEADER_NAME,
		HEADER_VALUE,
		HEADER_LINE_LF,
		HEADER_END_LF,

		LWS_START = 300,
		LWS_LF,
		LWS_SP_HT_START,
		LWS_SP_HT,

		// message-content
		CONTENT_START = 400,
		CONTENT,
	}; // }}}

public:
	std::function<void(buffer_ref&&, buffer_ref&&, buffer_ref&&, int, int)> on_request;
	std::function<void(buffer_ref&&, buffer_ref&&, buffer_ref&&)> on_response;

	std::function<void(buffer_ref&&, buffer_ref&&)> on_header;
	std::function<void(buffer_ref&&)> on_content;

	std::function<bool()> on_complete;

public:
	message_parser();

	void reset(state s = MESSAGE_BEGIN);
	std::size_t parse(buffer_ref&& chunk);
	std::size_t parse(buffer_ref&& chunk, std::error_code& ec);

private:
	void pass_request();
	void pass_header();
	bool pass_content(buffer_ref&& chunk, std::error_code& ec, std::size_t& nparsed);

private:
	state state_;

	// request-line
	buffer_ref method_;
	buffer_ref entity_;
	buffer_ref protocol_;
	int version_major_;
	int version_minor_;

	// current parsed header
	buffer_ref name_;
	buffer_ref value_;

	// body
	bool content_chunked_;            //!< whether or not request content is chunked encoded
	ssize_t content_length_;          //!< content length of whole content or current chunk
	chunked_decoder chunked_decoder_;
	chain_filter filter_chain_;
};

/*
class request_parser : public message_parser
{
public:
	std::size_t parse(buffer_ref&& chunk, std::error_code& ec)
	{
		std::size_t offset = 0;

		while (i != e)
		{
			switch (state_)
			{
				case METHOD_START:
					// ...
				default:
					return offset + message_parser::parse(chunk.ref(offset), ec);
			}
			++i;
			++offset;
		}
	}

	void reset()
	{
		message_parser::reset();
		// ...
	}
};
*/

} // namespace x0

// {{{ inlines
#if 0
#	define TRACE(msg...)
#else
#	define TRACE(msg...) DEBUG("message_parser: " msg)
#endif

namespace x0 {

inline message_parser::message_parser() :
	state_(MESSAGE_BEGIN),
	method_(),
	entity_(),
	protocol_(),
	version_major_(0),
	version_minor_(0),
	name_(),
	value_(),
	content_chunked_(false),
	content_length_(-1)
{
}

inline void message_parser::reset(state s)
{
	state_ = s;

	method_.clear();
	entity_.clear();
	protocol_.clear();
	version_major_ = 0;
	version_minor_ = 0;

	name_.clear();
	value_.clear();

	content_chunked_ = false;
	content_length_ = -1;
	chunked_decoder_.reset();
	filter_chain_.clear();
}

static inline const char *state2str(message_parser::state s)
{
	switch (s) {
		// artificial
		case message_parser::SYNTAX_ERROR: return "syntax-error";
		case message_parser::MESSAGE_BEGIN: return "message-begin";
		case message_parser::MESSAGE_END: return "message-end";

		// request-line
		case message_parser::REQUEST_LINE_START: return "request-line-start";
		case message_parser::METHOD: return "method";
		case message_parser::ENTITY_START: return "entity-start";
		case message_parser::ENTITY: return "entity";
		case message_parser::PROTOCOL_START: return "protocol-start";
		case message_parser::PROTOCOL: return "protocol";
		case message_parser::VERSION_MAJOR: return "version-major";
		case message_parser::VERSION_MINOR: return "version-minor";
		case message_parser::REQUEST_LINE_LF: return "request-line-lf";

		// Status-Line
		case message_parser::STATUS_LINE_START: return "status-line-start";
		case message_parser::STATUS_PROTOCOL_START: return "status-protocol-start";
		case message_parser::STATUS_PROTOCOL: return "status-protocol";
		case message_parser::STATUS_CODE: return "status-code";
		case message_parser::STATUS_MESSAGE_START: return "status-message-start";
		case message_parser::STATUS_MESSAGE: return "status-message";

		// message header
		case message_parser::HEADER_NAME_START: return "header-name-start";
		case message_parser::HEADER_NAME: return "header-name";
		case message_parser::HEADER_VALUE: return "header-value";
		case message_parser::LWS_START: return "lws-start";
		case message_parser::LWS_LF: return "lws-lf";
		case message_parser::LWS_SP_HT_START: return "lws-sp-ht-start";
		case message_parser::LWS_SP_HT: return "lws-sp-ht";
		case message_parser::HEADER_LINE_LF: return "header-line-lf";
		case message_parser::HEADER_END_LF: return "header-end-lf";

		// message content
		case message_parser::CONTENT_START: return "content-start";
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

	//TRACE("parse: size: %ld", chunk.size());

	/*
	 * CR               = 0x0D
	 * LF               = 0x0A
	 * SP               =
	 * HT               =
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
	 * absoluteURI      = ...
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

	enum {
		CR = 0x0D,
		LF = 0x0A,
		SP = 0x20,
		HT = 0x09,
	};

	if (state_ == CONTENT)
	{
		if (!pass_content(std::move(chunk), ec, offset))
			return offset;

		i += offset;
	}

	while (i != e)
	{
#if 0
		if (std::isprint(*i))
			TRACE("parse: offset: %4ld, character:  '%c', state: %s", offset, *i, state2str(state_));
		else                                            
			TRACE("parse: offset: %4ld, character: 0x%02X, state: %s", offset, *i, state2str(state_));
#endif

		switch (state_)
		{
			case MESSAGE_BEGIN:
				state_ = REQUEST_LINE_START;
				break;
			case REQUEST_LINE_START:
				if (std::isprint(*i))
				{
					state_ = METHOD;
					method_ = chunk.ref(offset, 1);

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case METHOD:
				if (*i == ' ')
				{
					state_ = ENTITY_START;

					++offset;
					++i;
				}
				else if (!std::isprint(*i))
					state_ = SYNTAX_ERROR;
				else
				{
					method_.shr();

					++offset;
					++i;
				}
				break;
			case ENTITY_START:
				if (std::isprint(*i))
				{
					entity_ = chunk.ref(offset, 1);
					state_ = ENTITY;

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case ENTITY:
				if (*i == ' ')
				{
					state_ = PROTOCOL_START;

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
			case PROTOCOL_START:
				if (std::isalpha(*i))
				{
					protocol_ =  chunk.ref(offset, 1);
					state_ = PROTOCOL;

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case PROTOCOL:
				if (*i == '/')
				{
					state_ = VERSION_MAJOR;

					++offset;
					++i;
				}
				else if (std::isalpha(*i))
				{
					protocol_.shr();

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case VERSION_MAJOR:
				if (std::isdigit(*i))
				{
					version_major_ = version_major_ * 10 + (*i - '0');

					++offset;
					++i;
				}
				else if (*i == '.')
				{
					state_ = VERSION_MINOR;

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case VERSION_MINOR:
				if (std::isdigit(*i))
				{
					version_minor_ = version_minor_ * 10 + (*i - '0');

					++offset;
					++i;
				}
				else if (*i == '\r')
				{
					state_ = REQUEST_LINE_LF;

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case REQUEST_LINE_LF:
				if (*i == '\n')
				{
					pass_request();
					state_ = HEADER_NAME_START;

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case STATUS_LINE_START:
			case STATUS_PROTOCOL_START:
			case STATUS_PROTOCOL:
			case STATUS_CODE:
			case STATUS_MESSAGE_START:
			case STATUS_MESSAGE:
				state_ = SYNTAX_ERROR; //! \todo impl
				break;
			case HEADER_NAME_START:
				if (std::isprint(*i)) {
					state_ = HEADER_NAME;
					name_ = chunk.ref(offset, 1);

					++offset;
					++i;
				}
				else if (*i == '\r')
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
					state_ = LWS_START;

					++offset;
					++i;
				}
				else if (std::isprint(*i))
				{
					name_.shr();

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case LWS_START:
				if (*i == '\r')
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
				if (*i == '\n')
				{
					state_ = LWS_SP_HT_START;

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case LWS_SP_HT_START:
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
					state_ = HEADER_NAME_START; // XXX no offset/i-update

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
				if (*i == '\r')
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
			case HEADER_LINE_LF:
				if (*i == '\n')
				{
					pass_header();

					state_ = HEADER_NAME_START;

					++offset;
					++i;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case HEADER_END_LF:
				if (*i == '\n')
				{
					state_ = CONTENT_START;

					++offset;
					++i;
				} else
					state_ = SYNTAX_ERROR;
				break;
			case CONTENT_START:
				if (content_length_ <= 0 && !content_chunked_) {
					state_ = SYNTAX_ERROR;
					break;
				}
				state_ = CONTENT;
			case CONTENT:
				pass_content(chunk.ref(offset), ec, offset);
				return offset;
			case MESSAGE_END:
				return offset;
			case SYNTAX_ERROR:
			{
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
	return offset;
}

inline void message_parser::pass_request()
{
	TRACE("request-line: method=%s, entity=%s, protocol=%s, vmaj=%d, vmin=%d",
			method_.str().c_str(), entity_.str().c_str(), protocol_.str().c_str(), version_major_, version_minor_);

	if (on_request)
		on_request(std::move(method_), std::move(entity_), std::move(protocol_), version_major_, version_minor_);
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

} // namespace x0

#undef TRACE

// }}}

#endif
