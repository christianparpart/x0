#include <x0/request_parser.hpp>

namespace x0 {

void request_parser::on_request(buffer_ref&& method, buffer_ref&& uri, buffer_ref&& protocol, int major, int minor)
{
	request_->method = std::move(method);

	request_->uri = std::move(uri);
	url_decode(request_->uri);

	std::size_t n = request_->uri.find("?");
	if (n != std::string::npos)
	{
		request_->path = request_->uri.ref(0, n);
		request_->query = request_->uri.ref(n + 1);
	}
	else
	{
		request_->path = request_->uri;
	}

	//request_->protocol = std::move(protocol);
	request_->http_version_major = major;
	request_->http_version_minor = major;
}

void request_parser::on_header(buffer_ref&& name, buffer_ref&& value)
{
	request_->headers.push_back(request_header(std::move(name), std::move(value)));
}

void request_parser::on_header_done()
{
	DEBUG("request_parser.on_header_done()");
	completed_ = true;
	parser_.abort();
}

void request_parser::on_content(buffer_ref&& chunk)
{
	if (request_->on_content)
		request_->on_content(std::move(chunk));
}

bool request_parser::on_complete()
{
	DEBUG("request_parser.on_complete()");
	completed_ = true;
	return false;
}

} // namespace x0
