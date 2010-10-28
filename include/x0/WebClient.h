/* <x0/WebClient.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_web_client_hpp
#define sw_x0_web_client_hpp (1)

#include <x0/Api.h>
#include <x0/Buffer.h>
#include <x0/http/HttpMessageProcessor.h>
#include <ev++.h>
#include <functional>
#include <system_error>

namespace x0 {

class X0_API WebClientBase :
	public HttpMessageProcessor
{
public:
	enum State {
		DISCONNECTED,
		CONNECTING,
		CONNECTED,
		WRITING,
		READING
	};

private:
	struct ev_loop *loop_;
	int fd_;
	State state_;
	ev::io io_;
	ev::timer timer_;
	std::error_code lastError_;
	Buffer requestBuffer_;
	std::size_t requestOffset_;
	std::size_t requestCount_;
	Buffer responseBuffer_;

private:
	void io(ev::io& w, int revents);
	void timeout(ev::timer& watcher, int revents);

	void onConnectComplete();

	void writeSome();
	void readSome();

	void startRead();
	void startWrite();

private:
	// from HttpMessageProcessor
	virtual void messageBegin(int version_major, int version_minor, int code, BufferRef&& text);
	virtual void messageHeader(BufferRef&& name, BufferRef&& value);
	virtual bool messageContent(BufferRef&& chunk);
	virtual bool messageEnd();

public:
	explicit WebClientBase(struct ev_loop *loop);
	~WebClientBase();

	// connection handling
	bool open(const std::string& host, int port);
	bool isOpen() const;
	void close();

	State state() const;
	std::error_code lastError() const;

	// timeouts (values <= 0 means disabled)
	int connectTimeout;
	int writeTimeout;
	int readTimeout;
	int keepaliveTimeout;

	// request handling
	template<typename method_type, typename path_type>
	void writeRequest(method_type&& method, path_type&& path);

	template<typename method_type, typename path_type, typename query_type>
	void writeRequest(method_type&& method, path_type&& path, query_type&& query);

	template<typename key_type, typename value_type>
	void writeHeader(key_type&& key, value_type&& value);

	template<typename handler_type>
	void setupContentWriter(handler_type&& handler);

	void commit(bool flush = true);

	void pause();
	void resume();

	template<typename chunk_type>
	ssize_t write(chunk_type&& chunk, bool last);

public:
	virtual void onConnect() = 0;
	virtual void onResponse(int vmajor, int vminor, int code, BufferRef&& message) = 0;
	virtual void onHeader(BufferRef&& name, BufferRef&& value) = 0;
	virtual bool onContentChunk(BufferRef&& chunk) = 0;
	virtual bool onComplete() = 0;
};

class X0_API WebClient : public WebClientBase
{
public:
	explicit WebClient(struct ev_loop *loop);

	std::function<void()> on_connect;
	std::function<void(int, int, int, BufferRef&&)> on_response;
	std::function<void(BufferRef&&, BufferRef&&)> on_header;
	std::function<bool(BufferRef&&)> on_content;
	std::function<bool()> on_complete;

private:
	virtual void onConnect();
	virtual void onResponse(int code, int vmajor, int vminor, BufferRef&& message);
	virtual void onHeader(BufferRef&& name, BufferRef&& value);
	virtual bool onContentChunk(BufferRef&& chunk);
	virtual bool onComplete();
};

} // namespace x0

// {{{ impl
namespace x0 {

inline WebClientBase::State WebClientBase::state() const
{
	return state_;
}

/** passes the request line into the buffer.
 *
 * \param method e.g. GET or POST
 * \param path the request path (including possible query args)
 */
template<typename method_type, typename path_type>
void WebClientBase::writeRequest(method_type&& method, path_type&& path)
{
	requestBuffer_.push_back(method);
	requestBuffer_.push_back(' ');
	requestBuffer_.push_back(path);
	requestBuffer_.push_back(' ');
	requestBuffer_.push_back("HTTP/1.1");
	requestBuffer_.push_back("\015\012");
}

/** passes the request line into the buffer.
 *
 * \param method e.g. GET or POST
 * \param path the request path (including possible query args)
 * \param query the query args (encoded)
 */
template<typename method_type, typename path_type, typename query_type>
void WebClientBase::writeRequest(method_type&& method, path_type&& path, query_type&& query)
{
	requestBuffer_.push_back(method);
	requestBuffer_.push_back(' ');
	requestBuffer_.push_back(path);
	requestBuffer_.push_back('?');
	requestBuffer_.push_back(query);
	requestBuffer_.push_back(' ');
	requestBuffer_.push_back("HTTP/1.1");
	requestBuffer_.push_back("\015\012");
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
void WebClientBase::writeHeader(key_type&& key, value_type&& value)
{
	requestBuffer_.push_back(key);
	requestBuffer_.push_back(": ");
	requestBuffer_.push_back(value);
	requestBuffer_.push_back("\015\012");
}

/** installs the content write handler.
 *
 * \param handler the callback to be invoked every time data can
 *                be written without blocking.
 */
template<typename handler_type>
void WebClientBase::setupContentWriter(handler_type&& handler)
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
ssize_t WebClientBase::write(chunk_type&& chunk, bool last)
{
	//! \todo implementation
	return 0;
}

} // namespace x0
// }}}

#endif
