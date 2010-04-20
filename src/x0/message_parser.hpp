#ifndef sw_x0_message_parser_hpp
#define sw_x0_message_parser_hpp (1)

#include <x0/buffer.hpp>
#include <x0/buffer_ref.hpp>
#include <x0/defines.hpp>
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
		REQUEST_LINE_START = 100,
		METHOD,
		ENTITY_START,
		ENTITY,
		PROTOCOL_START,
		PROTOCOL,
		VERSION_MAJOR,
		VERSION_MINOR,

		REQUEST_LINE_LF = 200,
		HEADER_NAME_START,
		HEADER_NAME,
		HEADER_VALUE_START,
		HEADER_VALUE,
		HEADER_VALUE_LWS,
		HEADER_LINE_LF,
		HEADER_END_LF,

		CONTENT_START = 300,
		CONTENT,

		// cosmetical
		SYNTAX_ERROR,
		BEGIN = REQUEST_LINE_START,
	}; // }}}

public:
	message_parser();

	std::function<void(buffer_ref&&, buffer_ref&&, buffer_ref&&, int, int)> on_request;
	std::function<void(buffer_ref&&, buffer_ref&&, buffer_ref&&)> on_response;
	std::function<void(buffer_ref&&, buffer_ref&&)> on_header;
	std::function<void(buffer_ref&&)> on_content;

	void reset(state s = BEGIN);
	std::size_t parse(buffer_ref&& chunk, std::error_code& ec);

private:
	void pass_request();
	void pass_header();
	std::size_t pass_content(buffer_ref&& chunk, std::error_code& ec);

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
	bool content_chunked_;   //!< whether or not request content is chunked encoded
	ssize_t content_length_; //!< content length of whole content or current chunk
};

} // namespace x0

// {{{ inlines
#if 0
#	define TRACE(msg...)
#else
#	define TRACE(msg...) DEBUG("message_parser: " msg)
#endif

namespace x0 {

inline message_parser::message_parser() :
	state_(BEGIN),
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
}

static inline const char *state2str(message_parser::state s)
{
	switch (s) {
		case message_parser::REQUEST_LINE_START: return "request-line-start";
		case message_parser::METHOD: return "method";
		case message_parser::ENTITY_START: return "entity-start";
		case message_parser::ENTITY: return "entity";
		case message_parser::PROTOCOL_START: return "protocol-start";
		case message_parser::PROTOCOL: return "protocol";
		case message_parser::VERSION_MAJOR: return "version-major";
		case message_parser::VERSION_MINOR: return "version-minor";
		case message_parser::REQUEST_LINE_LF: return "request-line-lf";
		case message_parser::HEADER_NAME_START: return "header-name-start";
		case message_parser::HEADER_NAME: return "header-name";
		case message_parser::HEADER_VALUE_START: return "header-value-start";
		case message_parser::HEADER_VALUE: return "header-value";
		case message_parser::HEADER_VALUE_LWS: return "header-value-lws";
		case message_parser::HEADER_LINE_LF: return "header-line-lf";
		case message_parser::HEADER_END_LF: return "header-end-lf";
		case message_parser::CONTENT_START: return "content-start";
		case message_parser::CONTENT: return "content";
		case message_parser::SYNTAX_ERROR: return "syntax-error";
	}
	return "UNKNOWN";
}

inline std::size_t message_parser::parse(buffer_ref&& chunk, std::error_code& ec)
{
	const char *i = chunk.begin();
	const char *e = chunk.end();
	std::size_t offset = 0;

	TRACE("parse: size: %ld", chunk.size());

	/*
	 * CR               = 0x0D
	 * LF               = 0x0A
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
	 * start-line       = Request-Line | Status-Line
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

	while (i != e)
	{
		if (std::isprint(*i))
			TRACE("parse: offset: %4ld, character:  '%c', state: %s", offset, *i, state2str(state_));
		else                                            
			TRACE("parse: offset: %4ld, character: 0x%02X, state: %s", offset, *i, state2str(state_));

		switch (state_)
		{
			case REQUEST_LINE_START:
				if (std::isprint(*i))
				{
					state_ = METHOD;
					method_ = chunk.ref(offset, 1);
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case METHOD:
				if (*i == ' ')
					state_ = ENTITY_START;
				else if (!std::isprint(*i))
					state_ = SYNTAX_ERROR;
				else
					method_.shr();
				break;
			case ENTITY_START:
				if (std::isprint(*i))
				{
					entity_ = chunk.ref(offset, 1);
					state_ = ENTITY;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case ENTITY:
				if (*i == ' ')
					state_ = PROTOCOL_START;
				else if (std::isprint(*i))
					entity_.shr();
				else
					state_ = SYNTAX_ERROR;
				break;
			case PROTOCOL_START:
				if (std::isalpha(*i))
				{
					protocol_ =  chunk.ref(offset, 1);
					state_ = PROTOCOL;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case PROTOCOL:
				if (*i == '/')
					state_ = VERSION_MAJOR;
				else if (std::isalpha(*i))
					protocol_.shr();
				else
					state_ = SYNTAX_ERROR;
				break;
			case VERSION_MAJOR:
				if (std::isdigit(*i))
					version_major_ = version_major_ * 10 + (*i - '0');
				else if (*i == '.')
					state_ = VERSION_MINOR;
				else
					state_ = SYNTAX_ERROR;
				break;
			case VERSION_MINOR:
				if (std::isdigit(*i))
					version_minor_ = version_minor_ * 10 + (*i - '0');
				else if (*i == '\r')
					state_ = REQUEST_LINE_LF;
				else
					state_ = SYNTAX_ERROR;
				break;
			case REQUEST_LINE_LF:
				if (*i == '\n')
				{
					pass_request();
					state_ = HEADER_NAME_START;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case HEADER_NAME_START:
				if (std::isprint(*i)) {
					state_ = HEADER_NAME;
					name_ = chunk.ref(offset, 1);
				}
				else if (*i == '\r')
					state_ = HEADER_END_LF;
				else
					state_ = SYNTAX_ERROR;
				break;
			case HEADER_NAME:
				if (*i == ':')
					state_ = HEADER_VALUE_START;
				else if (std::isprint(*i))
					name_.shr();
				else
					state_ = SYNTAX_ERROR;
				break;
			case HEADER_VALUE_START:
				if (std::isprint(*i))
				{
					state_ = HEADER_VALUE;
					value_ = chunk.ref(offset, 1);
				}
				else if (*i != ' ')
					state_ = SYNTAX_ERROR;
				break;
			case HEADER_VALUE:
				if (*i == '\r')
					state_ = HEADER_VALUE_LWS;
				else if (std::isprint(*i))
					value_.shr();
				else
					state_ = SYNTAX_ERROR;
				break;
			case HEADER_VALUE_LWS_START:
				if (*i == '\n')
					state_ = HEADER_VALUE_LWS;
				break;
			case HEADER_LINE_LF:
				if (*i == '\n')
				{
					pass_header();

					state_ = HEADER_NAME_START;
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case HEADER_END_LF:
				if (*i == '\n')
				{
					TRACE("all headers parsed");
					state_ = CONTENT_START;
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
				return offset + pass_content(chunk.ref(offset), ec);
			case SYNTAX_ERROR:
			{
#if !defined(NDEBUG)
				--offset;
				--i;

				if (std::isprint(*i))
					TRACE("parse: syntax error at offset: %ld, character: '%c'", offset, *i);
				else
					TRACE("parse: syntax error at offset: %ld, character: 0x%02X", offset, *i);

				return offset;
#else
				return offset - 1;
#endif
			}
		}

		++offset;
		++i;
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
}

inline std::size_t message_parser::pass_content(buffer_ref&& chunk, std::error_code& ec)
{
	// crop if chunk too large
	if (chunk.size() > static_cast<std::size_t>(content_length_))
		chunk.shr(-(chunk.size() - content_length_));

	TRACE("pass_content(%ld): '%s'", chunk.size(), chunk.str().c_str());

	content_length_ -= chunk.size();

	if (on_content)
		on_content(std::move(chunk));

	TRACE("pass_content: bytes left: %ld", content_length_);

	return chunk.size();
}

} // namespace x0

#undef TRACE

// }}}

#endif
