/* <x0/web_client.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2010 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_web_client_hpp
#define sw_x0_web_client_hpp (1)

#include <x0/api.hpp>
#include <x0/buffer.hpp>
#include <x0/response_parser.hpp>
#include <ev++.h>
#include <functional>

namespace x0 {

/** \brief HTTP/1.1 client API
 *
 */
class X0_API web_client
{
public:
	enum state_type {
		DISCONNECTED,
		CONNECTING,
		CONNECTED,
		WRITING,
		READING
	};

private:
	struct ev_loop *loop_;
	int fd_;
	state_type state_;
	ev::io io_;
	ev::timer timer_;
	std::string message_;
	buffer request_buffer_;
	std::size_t request_offset_;
	buffer response_buffer_;
	response_parser response_parser_;

private:
	void io(ev::io& w, int revents);
	void timeout(ev::timer& watcher, int revents);

	void connect_done();
	void write_some();
	void read_some();

	void start_read();
	void start_write();

	void _on_status(const buffer_ref& protocol, const buffer_ref& code, const buffer_ref& text);
	void _on_header(const buffer_ref& name, const buffer_ref& value);
	void _on_content(const buffer_ref& chunk);

public:
	explicit web_client(struct ev_loop *loop);
	~web_client();

	// connection handling
	void open(const std::string& host, int port);
	bool is_open() const;
	void close();

	state_type state() const;
	std::string message() const;

	// timeouts (values <= 0 means disabled)
	int connect_timeout;
	int write_timeout;
	int read_timeout;
	int keepalive_timeout;

	// request handling
	template<typename method_type, typename path_type>
	void pass_request(method_type&& method, path_type&& path);

	template<typename key_type, typename value_type>
	void pass_header(key_type&& key, value_type&& value);

	template<typename handler_type>
	void setup_content_writer(handler_type&& handler);

	void commit(bool flush = true);

	template<typename chunk_type>
	ssize_t pass_content(chunk_type&& chunk, bool last);

	// response handling
	std::function<void(const buffer_ref&, const buffer_ref&, const buffer_ref&)> on_response;
	std::function<void(const buffer_ref&, const buffer_ref&)> on_header;
	std::function<void(const buffer_ref&)> on_content;
	std::function<void()> on_complete;
};

} // namespace x0

// {{{ impl
namespace x0 {

inline web_client::state_type web_client::state() const
{
	return state_;
}

/** passes the request line into the buffer.
 *
 * \param method e.g. GET or POST
 * \param path the request path (including possible query args)
 */
template<typename method_type, typename path_type>
void web_client::pass_request(method_type&& method, path_type&& path)
{
	request_buffer_.push_back(method);
	request_buffer_.push_back(' ');
	request_buffer_.push_back(path);
	request_buffer_.push_back(' ');
	request_buffer_.push_back("HTTP/1.1");
	request_buffer_.push_back("\015\012");
}

/** passes a request header into the buffer.
 *
 * \param key header key field (e.g. Conent-Type)
 * \param valu  header value field (e.g. text/html)
 *
 * Do not pass connection control headers (e.g. Connection)
 * as these are passed automatically at the time of commit().
 *
 * The value also may not contain line feeds
 */
template<typename key_type, typename value_type>
void web_client::pass_header(key_type&& key, value_type&& value)
{
	request_buffer_.push_back(key);
	request_buffer_.push_back(": ");
	request_buffer_.push_back(value);
	request_buffer_.push_back("\015\012");
}

/** installs the content write handler.
 *
 * \param handler the callback to be invoked every time data can
 *                be written without blocking.
 */
template<typename handler_type>
void web_client::setup_content_writer(handler_type&& handler)
{
	//! \todo implementation
}

/** writes given content to the server.
 *
 * \param chunk chunk of data to transfer
 * \param last boolean indicating whether this was the last chunk of the body.
 * \return the number of bytes written, or -1 on error
 */
template<typename chunk_type>
ssize_t web_client::pass_content(chunk_type&& chunk, bool last)
{
	//! \todo implementation
	return 0;
}

} // namespace x0
// }}}

#endif
