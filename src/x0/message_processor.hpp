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

	std::error_code process(buffer_ref&& chunk, std::size_t& nparsed);
	std::size_t next_offset() const;

	void clear();

private:
	void reset();
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

inline std::size_t message_processor::next_offset() const
{
	return next_offset_;
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
