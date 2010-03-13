#ifndef x0_response_parser_hpp
#define x0_response_parser_hpp (1)

#include <x0/io/buffer.hpp>
#include <x0/io/buffer_ref.hpp>
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
	enum state_type
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
	};

public:
	//! process response status line
	std::function<void(const buffer_ref&, const buffer_ref&, const buffer_ref&)> status;

	//! process response header (name and value)
	std::function<void(const buffer_ref&, const buffer_ref&)> assign_header;

	//! process response body chunks
	std::function<void(const buffer_ref&)> process_content;

public:
	explicit response_parser(state_type state = ALL);

	void parse(const buffer_ref& chunk);
	void reset(state_type state = ALL);

private:
	state_type state_;
	std::size_t protocol_offset_, protocol_size_;
	std::size_t name_offset_, name_size_;
	std::size_t value_offset_, value_size_;
};

// {{{ inlines
inline response_parser::response_parser(state_type state) :
	state_(state),
	protocol_offset_(0), protocol_size_(0),
	name_offset_(0), name_size_(0),
	value_offset_(0), value_size_(0)
{
}

inline void response_parser::reset(state_type state)
{
	state_ = state;

	name_offset_ = 0;
	name_size_ = 0;

	value_offset_ = 0;
	value_size_ = 0;
}

/** parses (possibly partial) response chunk.
 *
 * \see status, assign_header, process_content 
 */
inline void response_parser::parse(const buffer_ref& chunk)
{
	//::write(STDERR_FILENO, first, last - first);
	//printf("\nSTART PROCESSING:\n");

	if (state_ == processing_content)
		return process_content(chunk);

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
				status(buf.ref(protocol_offset_, protocol_size_), buf.ref(name_offset_, name_size_), buf.ref(value_offset_, value_size_));
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
				status(buf.ref(protocol_offset_, protocol_size_), buf.ref(name_offset_, name_size_), buf.ref(value_offset_, value_size_));
			} else
				++value_size_;
			break;
		case parsing_status_lf:
			if (*first != '\n') {
				state_ = parsing_status_text;
				++value_size_;
			} else {
				state_ = parsing_header_name_begin;
				status(buf.ref(protocol_offset_, protocol_size_), buf.ref(name_offset_, name_size_), buf.ref(value_offset_, value_size_));
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
			process_content(buf.ref(offset, chunk.size() - (offset - chunk.offset())));
			return;
		case parsing_end:
			return;
		}

		++offset;
		++first;
	}
}
// }}}

} // namespace x0

#endif
