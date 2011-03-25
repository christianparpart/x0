/* <x0/HttpMessageProcessor.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpMessageProcessor.h>

namespace x0 {

#if 1
#	define TRACE(msg...) DEBUG("HttpMessageProcessor: " msg)
#else
#	define TRACE(msg...)
#endif

//! Support messages using LF-only as linefeed instead of CRLF,
//! which is not HTTP conform.
#define X0_HTTP_SUPPORT_SHORT_LF 1

// {{{ const std::error_category& http_message_category() throw()
class http_message_category_impl :
	public std::error_category
{
private:
	std::string codes_[32];

	void set(HttpMessageError ec, const std::string& message)
	{
		codes_[static_cast<int>(ec)] = message;
	}

	void initialize_codes()
	{
		for (std::size_t i = 0; i < sizeof(codes_) / sizeof(*codes_); ++i)
			codes_[i] = "Undefined";

		set(HttpMessageError::Success, "Success");
		set(HttpMessageError::Partial, "Partial");
		set(HttpMessageError::Aborted, "Aborted");
		set(HttpMessageError::SyntaxError, "Invalid Syntax");
	}

public:
	http_message_category_impl()
	{
		initialize_codes();
	}

	virtual const char *name() const
	{
		return "HttpMessage";
	}

	virtual std::string message(int ec) const
	{
		return codes_[ec];
	}
};

http_message_category_impl http_message_category_impl_;

const std::error_category& http_message_category() throw()
{
	return http_message_category_impl_;
}
// }}}

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
HttpMessageProcessor::HttpMessageProcessor(ParseMode mode) :
	mode_(mode),
	state_(MESSAGE_BEGIN),
	method_(),
	entity_(),
	versionMajor_(),
	versionMinor_(),
	code_(0),
	message_(),
	name_(),
	value_(),
	chunked_(false),
	contentLength_(-1),
	filters_()
{
}

inline bool HttpMessageProcessor::isChar(char value)
{
	return static_cast<unsigned>(value) >= 0 && static_cast<unsigned>(value) <= 127;
}

inline bool HttpMessageProcessor::isControl(char value)
{
	return (value >= 0 && value <= 31) || value == 127;
}

inline bool HttpMessageProcessor::isSeparator(char value)
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

inline bool HttpMessageProcessor::isToken(char value)
{
	return isChar(value) && !(isControl(value) || isSeparator(value));
}

inline bool HttpMessageProcessor::isText(char value)
{
	// TEXT = <any OCTET except CTLs but including LWS>
	return !isControl(value) || value == SP || value == HT;
}

// {{{ HttpMessageProcessor hook stubs
/** hook, invoked for each HTTP/1.1 Request-Line, that has been fully parsed.
 *
 * \param method the request-method (e.g. GET or POST)
 * \param uri the requested URI (e.g. /index.html)
 * \param versionMajor HTTP major version (e.g. 0 for 0.9)
 * \param versionMinor HTTP minor version (e.g. 9 for 0.9)
 */
void HttpMessageProcessor::messageBegin(BufferRef&& method, BufferRef&& uri, int versionMajor, int versionMinor)
{
}

/** hook, invoked for each HTTP/1.1 Status-Line, that has been fully parsed.
 *
 * \param versionMajor HTTP major version (e.g. 0 for 0.9)
 * \param versionMinor HTTP minor version (e.g. 9 for 0.9)
 * \param code HTTP response status code (e.g. 200 or 404)
 * \param text HTTP response status text (e.g. "Ok" or "Not Found")
 */
void HttpMessageProcessor::messageBegin(int versionMajor, int versionMinor, int code, BufferRef&& text)
{
}

/** hook, invoked for each generic HTTP Message.
 */
void HttpMessageProcessor::messageBegin()
{
}

/** hook, invoked for each sequentially parsed HTTP header.
 */
void HttpMessageProcessor::messageHeader(BufferRef&& name, BufferRef&& value)
{
}

/** hook, invoked once all request headers have been fully parsed (no possible content parsed yet).
 *
 * \note The default behavior (if not overridden) is, to do nothing and return true.
 *
 * \retval true continue processing further content (if any)
 * \retval false abort message processing
 */
bool HttpMessageProcessor::messageHeaderEnd()
{
	return true;
}

/** hook, invoked for every chunk of message content being processed.
 *
 * \note The default behavior (if not overridden) is, to do nothing and return true.
 *
 * \retval true continue processing further content (if any)
 * \retval false abort message processing
 */
bool HttpMessageProcessor::messageContent(BufferRef&& chunk)
{
	return true;
}

/** hook, invoked once a fully HTTP message has been processed.
 *
 * \note The default behavior (if not overridden) is, to do nothing and return true.
 *
 * \retval true continue processing further content (if any)
 * \retval false abort message processing
 */
bool HttpMessageProcessor::messageEnd()
{
	return true;
}
// }}}

const char *HttpMessageProcessor::state_str() const
{
	switch (state_) {
		// artificial
		case HttpMessageProcessor::SYNTAX_ERROR: return "syntax-error";
		case HttpMessageProcessor::MESSAGE_BEGIN: return "message-begin";

		// request-line
		case HttpMessageProcessor::REQUEST_LINE_BEGIN: return "request-line-begin";
		case HttpMessageProcessor::REQUEST_METHOD: return "request-method";
		case HttpMessageProcessor::REQUEST_ENTITY_BEGIN: return "request-entity-begin";
		case HttpMessageProcessor::REQUEST_ENTITY: return "request-entity";
		case HttpMessageProcessor::REQUEST_PROTOCOL_BEGIN: return "request-protocol-begin";
		case HttpMessageProcessor::REQUEST_PROTOCOL_T1: return "request-protocol-t1";
		case HttpMessageProcessor::REQUEST_PROTOCOL_T2: return "request-protocol-t2";
		case HttpMessageProcessor::REQUEST_PROTOCOL_P: return "request-protocol-p";
		case HttpMessageProcessor::REQUEST_PROTOCOL_SLASH: return "request-protocol-slash";
		case HttpMessageProcessor::REQUEST_PROTOCOL_VERSION_MAJOR: return "request-protocol-version-major";
		case HttpMessageProcessor::REQUEST_PROTOCOL_VERSION_MINOR: return "request-protocol-version-minor";
		case HttpMessageProcessor::REQUEST_LINE_LF: return "request-line-lf";

		// Status-Line
		case HttpMessageProcessor::STATUS_LINE_BEGIN: return "status-line-begin";
		case HttpMessageProcessor::STATUS_PROTOCOL_BEGIN: return "status-protocol-begin";
		case HttpMessageProcessor::STATUS_PROTOCOL_T1: return "status-protocol-t1";
		case HttpMessageProcessor::STATUS_PROTOCOL_T2: return "status-protocol-t2";
		case HttpMessageProcessor::STATUS_PROTOCOL_P: return "status-protocol-t2";
		case HttpMessageProcessor::STATUS_PROTOCOL_SLASH: return "status-protocol-t2";
		case HttpMessageProcessor::STATUS_PROTOCOL_VERSION_MAJOR: return "status-protocol-version-major";
		case HttpMessageProcessor::STATUS_PROTOCOL_VERSION_MINOR: return "status-protocol-version-minor";
		case HttpMessageProcessor::STATUS_CODE_BEGIN: return "status-code-begin";
		case HttpMessageProcessor::STATUS_CODE: return "status-code";
		case HttpMessageProcessor::STATUS_MESSAGE_BEGIN: return "status-message-begin";
		case HttpMessageProcessor::STATUS_MESSAGE: return "status-message";
		case HttpMessageProcessor::STATUS_MESSAGE_LF: return "status-message-lf";

		// message header
		case HttpMessageProcessor::HEADER_NAME_BEGIN: return "header-name-begin";
		case HttpMessageProcessor::HEADER_NAME: return "header-name";
		case HttpMessageProcessor::HEADER_VALUE: return "header-value";
		case HttpMessageProcessor::HEADER_END_LF: return "header-end-lf";

		// LWS
		case HttpMessageProcessor::LWS_BEGIN: return "lws-begin";
		case HttpMessageProcessor::LWS_LF: return "lws-lf";
		case HttpMessageProcessor::LWS_SP_HT_BEGIN: return "lws-sp-ht-begin";
		case HttpMessageProcessor::LWS_SP_HT: return "lws-sp-ht";

		// message content
		case HttpMessageProcessor::CONTENT_BEGIN: return "content-begin";
		case HttpMessageProcessor::CONTENT: return "content";
		case HttpMessageProcessor::CONTENT_CHUNK_SIZE_BEGIN: return "content-chunk-size-begin";
		case HttpMessageProcessor::CONTENT_CHUNK_SIZE: return "content-chunk-size";
		case HttpMessageProcessor::CONTENT_CHUNK_LF1: return "content-chunk-lf1";
		case HttpMessageProcessor::CONTENT_CHUNK_BODY: return "content-chunk-body";
		case HttpMessageProcessor::CONTENT_CHUNK_LF2: return "content-chunk-lf2";
		case HttpMessageProcessor::CONTENT_CHUNK_CR3: return "content-chunk-cr3";
		case HttpMessageProcessor::CONTENT_CHUNK_LF3: return "content-chunk_lf3";
	}
	return "UNKNOWN";
}

/** processes a message-chunk.
 *
 * \param chunk   the chunk of bytes to process
 * \param ofp     reference, incremented by the number of bytes actually parsed and processed.
 *
 * \return        the error code describing the processing result.
 */
HttpMessageError HttpMessageProcessor::process(BufferRef&& chunk, std::size_t& ofp)
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
	 * abs_path         = "/" *CHAR
	 * authority        = ...
	 * token            = 1*<any CHAR except CTLs or seperators>
	 * separator        = "(" | ")" | "<" | ">" | "@"
	 *                  | "," | ";" | ":" | "\" | <">
	 *                  | "/" | "[" | "]" | "?" | "="
	 *                  | "{" | "}" | SP | HT
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

	std::size_t offsetBase = ofp; // safe the ofp value for later reuse
	std::size_t offset = 0;

	//TRACE("process(curState:%s): size: %ld: '%s'", state_str(), chunk.size(), chunk.str().c_str());
	TRACE("process(curState:%s): size: %ld", state_str(), chunk.size());

	if (state_ == CONTENT)
	{
		if (!passContent(std::move(chunk), offset, ofp))
			return HttpMessageError::Aborted;

		i += offset;
	}

	State lastState = static_cast<State>(0);

	while (i != e)
	{
		lastState = state_;
#if 0
		if (std::isprint(*i)) {
			TRACE("parse: %4ld, 0x%02X (%c),  %s", offset, *i, *i, state_str());
		} else {
			TRACE("parse: %4ld, 0x%02X,     %s", offset, *i, state_str());
		}
#endif

		switch (state_)
		{
			case MESSAGE_BEGIN:
				contentLength_ = -1;
				switch (mode_) {
					case REQUEST:
						state_ = REQUEST_LINE_BEGIN;
						versionMajor_ = 0;
						versionMinor_ = 0;
						break;
					case RESPONSE:
						state_ = STATUS_LINE_BEGIN;
						code_ = 0;
						break;
					case MESSAGE:
						state_ = HEADER_NAME_BEGIN;

						// an internet message has no special top-line,
						// so we just invoke the callback right away
						messageBegin();

						break;
				}
				break;
			case REQUEST_LINE_BEGIN:
				if (isToken(*i))
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
				else if (!isToken(*i))
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
					versionMajor_ = versionMajor_ * 10 + *i - '0';
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
#if defined(X0_HTTP_SUPPORT_SHORT_LF)
				else if (*i == LF)
				{
					state_ = HEADER_NAME_BEGIN;
					++offset;
					++i;

					//TRACE("request-line: method=%s, entity=%s, vmaj=%d, vmin=%d",
					//		method_.str().c_str(), entity_.str().c_str(), versionMajor_, versionMinor_);

					messageBegin(std::move(method_), std::move(entity_), versionMajor_, versionMinor_);
				}
#endif
				else if (!std::isdigit(*i))
					state_ = SYNTAX_ERROR;
				else
				{
					versionMinor_ = versionMinor_ * 10 + *i - '0';
					++offset;
					++i;
				}
				break;
			case REQUEST_LINE_LF:
				if (*i == LF)
				{
					state_ = HEADER_NAME_BEGIN;
					++offset;
					++i;

					//TRACE("request-line: method=%s, entity=%s, vmaj=%d, vmin=%d",
					//		method_.str().c_str(), entity_.str().c_str(), versionMajor_, versionMinor_);

					messageBegin(std::move(method_), std::move(entity_), versionMajor_, versionMinor_);
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
					versionMajor_ = versionMajor_ * 10 + *i - '0';
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
					versionMinor_ = versionMinor_ * 10 + *i - '0';
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
				if (isText(*i))
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
				if (isText(*i) && *i != CR && *i != LF)
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
					state_ = HEADER_NAME_BEGIN;
					++offset;
					++i;

					//TRACE("status-line: HTTP/%d.%d, code=%d, message=%s", versionMajor_, versionMinor_, code_, message_.str().c_str());
					messageBegin(versionMajor_, versionMinor_, code_, std::move(message_));
				}
				else
					state_ = SYNTAX_ERROR;
				break;
			case HEADER_NAME_BEGIN:
				if (isToken(*i)) {
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
#if defined(X0_HTTP_SUPPORT_SHORT_LF)
				else if (*i == LF)
				{
					bool contentExpected = 
						contentLength_ > 0 
						|| chunked_
						|| mode_ == MESSAGE;

					if (contentExpected)
						state_ = CONTENT_BEGIN;
					else
						state_ = MESSAGE_BEGIN;

					++offset;
					++i;

					ofp = offsetBase + offset;

					if (!messageHeaderEnd())
						return HttpMessageError::Aborted;

					if (!contentExpected) {
						if (!messageEnd())
							return HttpMessageError::Aborted;
					}
				}
#endif
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
				else if (isToken(*i))
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
					state_ = HEADER_NAME_BEGIN;
					// XXX no offset/i-update

					//TRACE("header: name='%s', value='%s'", name_.str().c_str(), value_.str().c_str());

					if (iequals(name_, "Content-Length"))
					{
						contentLength_ = value_.as<int>();
					}
					else if (iequals(name_, "Transfer-Encoding"))
					{
						if (iequals(value_, "chunked"))
						{
							chunked_ = true;
						}
					}

					messageHeader(std::move(name_), std::move(value_));

					name_.clear();
					value_.clear();
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
#if defined(X0_HTTP_SUPPORT_SHORT_LF)
				else if (*i == LF)
				{
					state_ = LWS_SP_HT_BEGIN;
					++offset;
					++i;
				}
#endif
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
					bool contentExpected = 
						contentLength_ > 0 
						|| chunked_
						|| mode_ == MESSAGE;

					if (contentExpected)
						state_ = CONTENT_BEGIN;
					else
						state_ = MESSAGE_BEGIN;

					++offset;
					++i;

					ofp = offsetBase + offset;

					if (!messageHeaderEnd())
						return HttpMessageError::Aborted;

					if (!contentExpected)
					{
						if (!messageEnd())
							return HttpMessageError::Aborted;
					}
				} else
					state_ = SYNTAX_ERROR;
				break;
			case CONTENT_BEGIN:
				if (chunked_)
					state_ = CONTENT_CHUNK_SIZE_BEGIN;
				else if (contentLength_ < 0 && mode_ != MESSAGE)
					state_ = SYNTAX_ERROR;
				else
					state_ = CONTENT;
				break;
			case CONTENT:
			{
				std::size_t nparsed = 0;

				ofp = offsetBase + offset;
				if (!passContent(chunk.ref(offset), nparsed, ofp))
					return HttpMessageError::Aborted;

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
				contentLength_ = 0;
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
					contentLength_ = contentLength_ * 16 + *i - '0';
					++offset;
					++i;
				}
				else if (*i >= 'a' && *i <= 'f')
				{
					contentLength_ = contentLength_ * 16 + 10 + *i - 'a';
					++offset;
					++i;
				}
				else if (*i >= 'A' && *i <= 'F')
				{
					contentLength_ = contentLength_ * 16 + 10 + *i - 'A';
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
					//TRACE("content_length: %ld", contentLength_);
					if (contentLength_ != 0)
						state_ = CONTENT_CHUNK_BODY;
					else
						state_ = CONTENT_CHUNK_CR3;

					++offset;
					++i;
				}
				break;
			case CONTENT_CHUNK_BODY:
				if (contentLength_)
				{
					std::size_t nparsed = 0;

					ofp = offsetBase + offset;
					if (!passContent(chunk.ref(offset), nparsed, ofp))
						return HttpMessageError::Aborted;

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

					ofp = offsetBase + offset;

					if (!messageEnd())
						return HttpMessageError::Aborted;

					state_ = MESSAGE_BEGIN;
				}
				break;
			case SYNTAX_ERROR:
			{
#if 1 // !defined(NDEBUG)
				TRACE("parse: syntax error (last state: %i)", lastState);
				if (std::isprint(*i)) {
					TRACE("parse: syntax error at offset: %ld, character: '%c'", offset, *i);
				} else {
					TRACE("parse: syntax error at offset: %ld, character: 0x%02X", offset, *i);
				}
				Buffer::dump(chunk.data(), chunk.size(), "request chunk");
#endif
				ofp = offsetBase + offset;
				return HttpMessageError::SyntaxError;
			}
			default:
#if !defined(NDEBUG)
				TRACE("parse: unknown state %i (last: %i)", state_, lastState);
				if (std::isprint(*i)) {
					TRACE("parse: internal error at offset: %ld, character: '%c'", offset, *i);
				} else {
					TRACE("parse: internal error at offset: %ld, character: 0x%02X", offset, *i);
				}
				Buffer::dump(chunk.data(), chunk.size(), "request chunk");
#endif
				return HttpMessageError::SyntaxError;
		}
	}
	// we've reached the end of the chunk

	if (state_ == CONTENT_BEGIN)
	{
		// we've just parsed all headers but no body yet.

		if (contentLength_ < 0 && !chunked_ && mode_ != MESSAGE)
		{
			// and there's no body to come
			ofp = offsetBase + offset;

			if (!messageEnd())
				return HttpMessageError::Aborted;

			// subsequent calls to parse() process next request(s).
			state_ = MESSAGE_BEGIN;
		}
	}

	ofp = offsetBase + offset;

	if (state_ != MESSAGE_BEGIN)
		return HttpMessageError::Partial;
	else
		return HttpMessageError::Success;
}

/** passes given content chunk to the callback.
 *
 * \param chunk the chunk of data to be passed to the callback.
 * \param ec
 * \param nparsed ref to the number of bytes parsed. will be updated.
 * \param ofp
 */
bool HttpMessageProcessor::passContent(BufferRef&& chunk, std::size_t& nparsed, std::size_t& ofp)
{
	if (contentLength_ > 0)
	{
		// shrink down to remaining content-length
		BufferRef c(chunk);
		if (chunk.size() > static_cast<std::size_t>(contentLength_))
			c.shr(-(c.size() - contentLength_));

		ofp += c.size();
		nparsed += c.size();
		contentLength_ -= c.size();

		//TRACE("passContent: chunk_size=%ld, bytes_left=%ld; '%s'",
		//		c.size(), contentLength_, c.str().c_str());

		if (chunked_)
		{
			Buffer result(c);

			if (!filters_.empty())
			{
				if (!messageContent(filters_.process(c)))
					return false;
			}
			else
			{
				if (!messageContent(std::move(c)))
					return false;
			}

			if (state_ == MESSAGE_BEGIN)
			{
				return messageEnd();
			}
		}
		else // fixed-size content (via "Content-Length")
		{
			bool rv = filters_.empty()
				? messageContent(std::move(c))
				: messageContent(filters_.process(c));

			if (contentLength_ == 0 && mode_ != MESSAGE)
				state_ = MESSAGE_BEGIN;

			if (!rv)
				return false;

			if (state_ == MESSAGE_BEGIN)
			{
				//TRACE("content fully parsed -> complete");
				return messageEnd();
			}
		}
	}
	else if (contentLength_ < 0) // no "Content-Length" and no "chunked transfer encoding" defined
	{
		//TRACE("passContent: chunk_size=%ld, infinite; '%s'",
		//		chunk.size(), chunk.str().c_str());

		ofp += chunk.size();
		nparsed += chunk.size();

		if (filters_.empty())
		{
			if (!messageContent(std::move(chunk)))
				return false;
		}
		else
		{
			if (!messageContent(filters_.process(chunk)))
				return false;
		}
	}

	return true;
}

} // namespace x0
