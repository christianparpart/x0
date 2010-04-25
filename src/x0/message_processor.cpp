#include <x0/message_processor.hpp>

namespace x0 {

// {{{ const std::error_category& http_message_category() throw()
class http_message_category_impl :
	public std::error_category
{
private:
	std::string codes_[32];

	void set(http_message_error ec, const std::string& message)
	{
		codes_[static_cast<int>(ec)] = message;
	}

	void initialize_codes()
	{
		for (std::size_t i = 0; i < sizeof(codes_) / sizeof(*codes_); ++i)
			codes_[i] = "Undefined";

		set(http_message_error::success, "Success");
		set(http_message_error::partial, "Partial");
		set(http_message_error::aborted, "Aborted");
		set(http_message_error::invalid_syntax, "Invalid Syntax");
	}

public:
	http_message_category_impl()
	{
		initialize_codes();
	}

	virtual const char *name() const
	{
		return "http_message";
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

// {{{ message_processor hook stubs
void message_processor::message_begin(buffer_ref&& method, buffer_ref&& entity, int version_major, int version_minor)
{
}

void message_processor::message_begin(int version_major, int version_minor, int code, buffer_ref&& text)
{
}

void message_processor::message_begin()
{
}

void message_processor::message_header(buffer_ref&& name, buffer_ref&& value)
{
}

bool message_processor::message_header_done()
{
	return true;
}

bool message_processor::message_content(buffer_ref&& chunk)
{
	return true;
}

bool message_processor::message_end()
{
	return true;
}
// }}}

} // namespace x0
