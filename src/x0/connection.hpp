/* <x0/connection.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_connection_hpp
#define x0_connection_hpp (1)

#include <x0/request_parser.hpp>
#include <x0/io/sink.hpp>
#include <x0/io/source.hpp>
#include <x0/io/async_writer.hpp>
#include <x0/buffer.hpp>
#include <x0/request.hpp>
#include <x0/server.hpp>
#include <x0/property.hpp>
#include <x0/types.hpp>
#include <x0/api.hpp>
#include <x0/sysconfig.h>

#if defined(WITH_SSL)
#	include <gnutls/gnutls.h>
#endif

#include <functional>
#include <memory>
#include <string>
#include <ev++.h>
#include <netinet/in.h> // sockaddr_in

namespace x0 {

//! \addtogroup core
//@{

class connection_sink;

/**
 * \brief represents an HTTP connection handling incoming requests.
 */
class connection
{
public:
	connection& operator=(const connection&) = delete;
	connection(const connection&) = delete;

	/**
	 * creates an HTTP connection object.
	 * \param srv a ptr to the server object this connection belongs to.
	 */
	explicit connection(x0::listener& listener);

	~connection();

	/** start first async operation for this connection.
	 *
	 * This is done by simply registering the underlying socket to the the I/O service
	 * to watch for available input.
	 * \see stop()
	 * :
	 */
	void start();

	/** Resumes async operations.
	 *
	 * This method is being invoked on a keep-alive connection to parse further requests.
	 * \see start()
	 */
	void resume();

	value_property<bool> secure;				//!< true if this is a secure (HTTPS) connection, false otherwise.

	int handle() const;							//!< Retrieves a reference to the connection socket.
	x0::server& server();						//!< Retrieves a reference to the server instance.

	std::string remote_ip() const;				//!< Retrieves the IP address of the remote end point (client).
	int remote_port() const;					//!< Retrieves the TCP port numer of the remote end point (client).

	std::string local_ip() const;
	int local_port() const;

	template<typename CompletionHandler>
	void on_ready(CompletionHandler callback, int events);

	const x0::listener& listener() const;

private:
	friend class response;
	friend class x0::listener;
	friend class connection_sink;

	void async_read_some();
	void async_write_some();

	void handle_read();
	void handle_write();
	void handle_timeout();

	void async_write(const source_ptr& buffer, const completion_handler_type& handler);

	void io_callback(ev::io& w, int revents);
	void timeout_callback(ev::timer& watcher, int revents);

#if defined(WITH_SSL)
	bool ssl_enabled() const;
	void ssl_initialize();
	bool ssl_handshake();
#endif

	struct ::ev_loop *loop() const;

	friend int connection_timer_offset();

public:
	std::map<plugin *, custom_data_ptr> custom_data;

private:
	x0::listener& listener_;
	x0::server& server_;					//!< server object owning this connection

	int socket_;							//!< underlying communication socket
	sockaddr_in6 saddr_;

	mutable std::string remote_ip_;			//!< internal cache to client ip
	mutable int remote_port_;				//!< internal cache to client port

	// HTTP request
	buffer buffer_;							//!< buffer for incoming data.
	request *request_;						//!< currently parsed http request 
	request_parser request_parser_;			//!< http request parser

#if defined(WITH_SSL)
	gnutls_session_t ssl_session_;			//!< SSL (GnuTLS) session handle

	enum {
		handshaking,
		requesting,
		responding
	} state_;
#endif

	ev::io watcher_;
	ev::timer timer_;						//!< deadline timer for detecting read/write timeouts.

	std::function<void(connection *)> write_some;
	std::function<void(connection *)> read_some;
};

// {{{ inlines
inline struct ::ev_loop* connection::loop() const
{
	return server_.loop();
}

inline int connection::handle() const
{
	return socket_;
}

inline server& connection::server()
{
	return server_;
}

/** write something into the connection stream.
 * \param buffer the buffer of bytes to be written into the connection.
 * \param handler the completion handler to invoke once the buffer has been either fully written or an error occured.
 */
inline void connection::async_write(const source_ptr& buffer, const completion_handler_type& handler)
{
	x0::async_write(this, buffer, handler);
}

template<typename CompletionHandler>
inline void connection::on_ready(CompletionHandler callback, int events)
{
	write_some = callback;

	if ((events & ev::WRITE) && server_.max_write_idle() != -1)
		timer_.start(server_.max_write_idle(), 0.0);
	else if ((events & ev::READ) && server_.max_read_idle() != -1)
		timer_.start(server_.max_read_idle(), 0.0);

//	if (server_.max_connection_idle() != -1)
//		timer_.start(server_.max_connection_idle(), 0.0);

	watcher_.set(socket_, events);
	watcher_.start();
}

inline const x0::listener& connection::listener() const
{
	return listener_;
}
// }}}

//@}

} // namespace x0

#endif
