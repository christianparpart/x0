#ifndef x0_response_parser_hpp
#define x0_response_parser_hpp (1)

#include <x0/buffer.hpp>
#include <x0/buffer_ref.hpp>
#include <x0/io/chain_filter.hpp>
#include <x0/io/chunked_decoder.hpp>
#include <x0/api.hpp>
#include <functional>

namespace x0 {

/** HTTP response parser.
 *
 * Should be used by CGI and proxy plugin for example.
 */
class X0_API response_parser
{
public:
	enum state_type // {{{
	{
		// response status line
		parsing_status_line_begin,
		parsing_status_protocol,
		parsing_status_ws1,
		parsing_status_code,
		parsing_status_ws2,
		parsing_status_text,
		parsing_status_lf,

		// response header
		parsing_header_name_begin,
		parsing_header_name,
		parsing_header_value_ws_left,
		parsing_header_value,
		expecting_lf1,
		expecting_cr2,
		expecting_lf2,

		// response body
		processing_content,

		//
		parsing_end,

		// cosmetical
		ALL = parsing_status_line_begin,
		SKIP_STATUS = parsing_header_name_begin
	}; // }}}

public:
	//! process response status line
	std::function<void(const buffer_ref&, const buffer_ref&, const buffer_ref&)> on_status;

	//! process response header (name and value)
	std::function<void(const buffer_ref&, const buffer_ref&)> on_header;

	//! process response body chunks
	std::function<void(const buffer_ref&)> on_content;

	//! invoked once response has been fully parsed
	std::function<void()> on_complete;

public:
	explicit response_parser(state_type state = ALL);

	std::size_t parse(buffer_ref&& chunk);
	void reset(state_type state = ALL);

private:
	void set_status(const buffer_ref& protocol, const buffer_ref& code, const buffer_ref& text);
	void assign_header(const buffer_ref& name, const buffer_ref& value);
	std::size_t process_content(buffer_ref&& chunk);

private:
	state_type state_;
	std::size_t protocol_offset_, protocol_size_;
	std::size_t name_offset_, name_size_;
	std::size_t value_offset_, value_size_;
	ssize_t content_length_;
	chain_filter filter_chain_;
};

// {{{ inlines
inline response_parser::response_parser(state_type state) :
	state_(state),
	protocol_offset_(0), protocol_size_(0),
	name_offset_(0), name_size_(0),
	value_offset_(0), value_size_(0),
	content_length_(-1),
	filter_chain_()
{
}

inline void response_parser::reset(state_type state)
{
	state_ = state;

	name_offset_ = 0;
	name_size_ = 0;

	value_offset_ = 0;
	value_size_ = 0;

	content_length_ = -1;
	filter_chain_.clear();
}

/** parses (possibly partial) response chunk.
 *
 * \see status, assign_header, process_content 
 */
inline std::size_t response_parser::parse(buffer_ref&& chunk)
{
	if (state_ == processing_content)
		return process_content(std::move(chunk));

	const char *first = chunk.begin();
	const char *last = chunk.end();
	std::size_t offset = chunk.offset();
	buffer& buf = chunk.buffer();

	while (first != last)
	{
		switch (state_)
		{
		case parsing_status_line_begin:
			state_ = parsing_status_protocol;
			protocol_offset_ = offset;
			protocol_size_ = 1;
			break;
		case parsing_status_protocol:
			if (*first == ' ')
				state_ = parsing_status_ws1;
			else
				++protocol_size_;
			break;
		case parsing_status_ws1:
			if (*first != ' ') {
				state_ = parsing_status_code;
				name_offset_ = offset;
				name_size_ = 1;
			}
			break;
		case parsing_status_code:
			if (*first == ' ')
				state_ = parsing_status_ws2;
			else if (*first == '\r')
				state_ = parsing_status_lf;
			else if (*first == '\n') {
				state_ = parsing_header_name_begin;
				set_status(buf.ref(protocol_offset_, protocol_size_), buf.ref(name_offset_, name_size_), buf.ref(value_offset_, value_size_));
			} else
				++name_size_;
			break;
		case parsing_status_ws2:
			if (*first != ' ') {
				state_ = parsing_status_text;
				value_offset_ = offset;
				value_size_ = 1;
			}
			break;
		case parsing_status_text:
			if (*first == '\r')
				state_ = parsing_status_lf;
			else if (*first == '\n') {
				state_ = parsing_header_name_begin;
				set_status(buf.ref(protocol_offset_, protocol_size_), buf.ref(name_offset_, name_size_), buf.ref(value_offset_, value_size_));
			} else
				++value_size_;
			break;
		case parsing_status_lf:
			if (*first != '\n') {
				state_ = parsing_status_text;
				++value_size_;
			} else {
				state_ = parsing_header_name_begin;
				set_status(buf.ref(protocol_offset_, protocol_size_), buf.ref(name_offset_, name_size_), buf.ref(value_offset_, value_size_));
			}
			break;
		case parsing_header_name_begin:
			state_ = parsing_header_name;
			name_offset_ = offset;
			name_size_ = 1;
			break;
		case parsing_header_name:
			if (*first == ':')
				state_ = parsing_header_value_ws_left;
			else if (*first == '\n')
				state_ = processing_content;
			else
				++name_size_;
			break;
		case parsing_header_value_ws_left:
			if (!std::isspace(*first)) {
				state_ = parsing_header_value;
				value_offset_ = offset;
				value_size_ = 1;
			}
			break;
		case parsing_header_value:
			if (*first == '\r') {
				state_ = expecting_lf1;
			} else if (*first == '\n') {
				assign_header(buf.ref(name_offset_, name_size_), buf.ref(value_offset_, value_size_));
				state_ = expecting_cr2;
			} else {
				++value_size_;
			}
			break;
		case expecting_lf1:
			if (*first == '\n') {
				assign_header(buf.ref(name_offset_, name_size_), buf.ref(value_offset_, value_size_));
				state_ = expecting_cr2;
			} else {
				++value_size_;
			}
			break;
		case expecting_cr2:
			if (*first == '\r')
				state_ = expecting_lf2;
			else if (*first == '\n')
				state_ = processing_content;
			else {
				state_ = parsing_header_name;
				name_offset_ = offset;
				name_size_ = 1;
			}
			break;
		case expecting_lf2:
			if (*first == '\n')					// [CR] LF [CR] LF
				state_ = processing_content;
			else {								// [CR] LF [CR] any
				state_ = parsing_header_name;
				name_offset_ = offset;
				name_size_ = 1;
			}
			break;
		case processing_content:
		{
			std::size_t size = first - chunk.begin();
			size += process_content(buf.ref(offset, chunk.size() - (offset - chunk.offset())));
			return size;
		}
		case parsing_end:
			return 0;
		}

		++offset;
		++first;
	}

	return chunk.size();
}

inline void response_parser::set_status(const buffer_ref& protocol, const buffer_ref& code, const buffer_ref& text)
{
	if (on_status)
		on_status(protocol, code, text);
}

inline void response_parser::assign_header(const buffer_ref& name, const buffer_ref& value)
{
	if (iequals(name, "Content-Length"))
		content_length_ = value.as<int>();
	else if (iequals(name, "Transfer-Encoding") && equals(value, "chunked"))
		filter_chain_.push_back(std::make_shared<chunked_decoder>());

	if (on_header)
		on_header(name, value);
}

inline std::size_t response_parser::process_content(buffer_ref&& chunk)
{
	if (content_length_ > 0) // fixed-size content
	{
		if (chunk.size() > static_cast<std::size_t>(content_length_))
		{
			// shrink down to remaining content-length
			chunk.shr(-(chunk.size() - content_length_));
		}

		content_length_ -= chunk.size();
	}

	if (filter_chain_.empty())
	{
		if (on_content)
			on_content(chunk);

		if (!content_length_)
		{
			state_ = parsing_end;

			if (on_complete)
				on_complete();
		}
	}
	else
	{
		buffer result(filter_chain_.process(chunk));

		if (!result.empty() && on_content)
			on_content(result);

		if (result.empty() || !content_length_)
		{
			state_ = parsing_end;

			if (on_complete)
				on_complete();
		}
	}

	return chunk.size();
}
// }}}

} // namespace x0

#endif
