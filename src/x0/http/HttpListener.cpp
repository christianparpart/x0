/* <x0/HttpListener.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpListener.h>
#include <x0/http/HttpConnection.h>
#include <x0/http/HttpServer.h>

#include <x0/sysconfig.h>

#if defined(WITH_SSL)
#	include <gnutls/gnutls.h>
#endif

#include <arpa/inet.h>		// inet_pton()
#include <netinet/tcp.h>	// TCP_QUICKACK, TCP_DEFER_ACCEPT
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

namespace x0 {

HttpListener::HttpListener(HttpServer& srv) : 
	watcher_(),
	fd_(-1),
	server_(srv),
	address_(),
	port_(-1),
	backlog_(SOMAXCONN)
#if defined(WITH_SSL)
	,
	secure_(false),
	ssl_db_(512),
	crl_file_(),
	trust_file_(),
	key_file_(),
	cert_file_()
#endif
{
	watcher_.set<HttpListener, &HttpListener::callback>(this);
}

HttpListener::~HttpListener()
{
	stop();
}

void HttpListener::stop()
{
	if (fd_ == -1)
		return;

	watcher_.stop();

	::close(fd_);
	fd_ = -1;

#if defined(WITH_SSL)
	if (secure())
	{
		gnutls_priority_deinit(priority_cache_);
		gnutls_certificate_free_credentials(x509_cred_);
		gnutls_dh_params_deinit(dh_params_);
	}
#endif
}

inline void HttpListener::setsockopt(int socket, int layer, int option, int value)
{
	if (::setsockopt(socket, layer, option, &value, sizeof(value)) < 0)
	{
		log(Severity::error, "Error setting socket option (fd=%d, layer=%d, opt=%d, val=%d): %s",
				socket, layer, option, value, strerror(errno));
	}
}

#if defined(WITH_SSL)
void HttpListener::secure(bool value)
{
	if (value == secure_)
		return;

	bool resume = active();
	if (resume) stop();

	secure_ = value;

	if (resume) start();
}

void HttpListener::crl_file(const std::string& value)
{
	if (value == crl_file_)
		return;

	bool resume = active();
	if (resume) stop();

	crl_file_ = value;

	if (resume) start();
}

void HttpListener::trust_file(const std::string& value)
{
	if (value == trust_file_)
		return;

	bool resume = active();
	if (resume) stop();

	trust_file_ = value;

	if (resume) start();
}

void HttpListener::key_file(const std::string& value)
{
	if (value == key_file_)
		return;

	bool resume = active();
	if (resume) stop();

	key_file_ = value;

	if (resume) start();
}

void HttpListener::cert_file(const std::string& value)
{
	if (value == cert_file_)
		return;

	bool resume = active();
	if (resume) stop();

	cert_file_ = value;

	if (resume) start();
}
#endif

void HttpListener::prepare()
{
#if defined(WITH_SSL)
	if (secure())
	{
		gnutls_priority_init(&priority_cache_, "NORMAL", NULL);

		gnutls_certificate_allocate_credentials(&x509_cred_);

		if (!trust_file_.empty())
			gnutls_certificate_set_x509_trust_file(x509_cred_, trust_file_.c_str(), GNUTLS_X509_FMT_PEM);

		if (!crl_file_.empty())
			gnutls_certificate_set_x509_crl_file(x509_cred_, crl_file_.c_str(), GNUTLS_X509_FMT_PEM);

		gnutls_certificate_set_x509_key_file(x509_cred_, cert_file_.c_str(), key_file_.c_str(), GNUTLS_X509_FMT_PEM);

		gnutls_dh_params_init(&dh_params_);
		gnutls_dh_params_generate2(dh_params_, 1024);

		gnutls_certificate_set_dh_params(x509_cred_, dh_params_);

		log(Severity::notice, "Start listening on [%s]:%d [secure]", address_.c_str(), port_);
	}
	else
		log(Severity::notice, "Start listening on [%s]:%d", address_.c_str(), port_);
#else
	log(Severity::notice, "Start listening on [%s]:%d", address_.c_str(), port_);
#endif

	fd_ = ::socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
	fcntl(fd_, F_SETFL, FD_CLOEXEC);

	if (fcntl(fd_, F_SETFL, O_NONBLOCK) < 0)
		log(Severity::error, "could not set server socket into non-blocking mode: %s\n", strerror(errno));

	sockaddr_in6 sin;
	bzero(&sin, sizeof(sin));

	sin.sin6_family = AF_INET6;
	sin.sin6_port = htons(port_);

	if (inet_pton(sin.sin6_family, address_.c_str(), sin.sin6_addr.s6_addr) < 0)
		log(Severity::error, "Could not resolve IP address (%s): %s", address_.c_str(), strerror(errno));

#if defined(SO_REUSEADDR)
	//! \todo SO_REUSEADDR: could be configurable
	setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, 1);
#endif

#if defined(TCP_QUICKACK)
	//! \todo TCP_QUICKACK: could be configurable
	setsockopt(fd_, SOL_TCP, TCP_QUICKACK, 1);
#endif

#if defined(TCP_DEFER_ACCEPT)
	setsockopt(fd_, SOL_TCP, TCP_DEFER_ACCEPT, 1);
#endif

//	acceptor_.set_option(asio::ip::tcp::acceptor::linger(false, 0));
//	acceptor_.set_option(asio::ip::tcp::acceptor::keep_alive(true));

	if (::bind(fd_, (sockaddr *)&sin, sizeof(sin)) < 0)
		log(Severity::error, "Cannot bind to IP-address (%s): %s", address_.c_str(), strerror(errno));

	if (::listen(fd_, backlog_) < 0)
		log(Severity::error, "Cannot listen to IP-address (%s): %s", address_.c_str(), strerror(errno));
}

void HttpListener::start()
{
	if (fd_ == -1)
		prepare();

	watcher_.start(fd_, ev::READ);
}

void HttpListener::callback(ev::io& watcher, int revents)
{
	// TODO accept() as much until it would block.
	HttpConnection *c = new HttpConnection(*this);

	c->start();
}

std::string HttpListener::address() const
{
	return address_;
}

void HttpListener::address(const std::string& value)
{
	address_ = value;
}

int HttpListener::port() const
{
	return port_;
}

void HttpListener::port(int value)
{
	port_ = value;
}

int HttpListener::backlog() const
{
	return backlog_;
}

void HttpListener::backlog(int value)
{
	backlog_ = value;
}

} // namespace x0
