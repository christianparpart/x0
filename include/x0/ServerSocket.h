#ifndef sw_x0_ServerSocket_h
#define sw_x0_ServerSocket_h

#include <x0/Api.h>
#include <x0/Defines.h>
#include <x0/Logging.h>
#include <string>
#include <ev++.h>
#include <sys/socket.h> // for isLocal() and isTcp()

namespace x0 {

class Socket;
class SocketSpec;
class SocketDriver;
class IPAddress;

class X0_API ServerSocket
#ifndef NDEBUG
	: public Logging
#endif
{
private:
	struct ev_loop* loop_;
	int flags_;
	int typeMask_;
	int backlog_;
	int addressFamily_;
	int fd_;
	ev::io io_;
	SocketDriver* socketDriver_;
	std::string errorText_;

	void (*callback_)(Socket*, ServerSocket*);
	void* callbackData_;

	std::string address_;
	int port_;

public:
	explicit ServerSocket(struct ev_loop* loop);
	~ServerSocket();

	void setBacklog(int value);
	int backlog() const { return backlog_; }

	bool open(const std::string& ipAddress, int port, int flags);
	bool open(const std::string& localAddress, int flags);
	bool open(const SocketSpec& spec, int flags);
	int handle() const { return fd_; }
	bool isOpen() const { return fd_ >= 0; }
	bool isStarted() const { return io_.is_active(); }
	void start();
	void stop();
	void close();

	int addressFamily() const { return addressFamily_; }
	bool isLocal() const { return addressFamily_ == AF_UNIX; }
	bool isTcp() const { return addressFamily_ == AF_INET || addressFamily_ == AF_INET6; }

	bool setFlags(unsigned flags, bool enable);

	void setSocketDriver(SocketDriver* sd);
	SocketDriver* socketDriver() { return socketDriver_; }
	const SocketDriver* socketDriver() const { return socketDriver_; }

	template<typename K, void (K::*cb)(Socket*, ServerSocket*)>
	void set(K* object);

	const std::string& errorText() const { return errorText_; }

	const std::string& address() const { return address_; }
	int port() const { return port_; }

private:
	template<typename K, void (K::*cb)(Socket*, ServerSocket*)>
	static void callback_thunk(Socket* cs, ServerSocket* ss);

	void accept(ev::io&, int);
};

// {{{
template<typename K, void (K::*cb)(Socket*, ServerSocket*)>
void ServerSocket::set(K* object)
{
	callback_ = &callback_thunk<K, cb>;
	callbackData_ = object;
}

template<typename K, void (K::*cb)(Socket*, ServerSocket*)>
void ServerSocket::callback_thunk(Socket* cs, ServerSocket* ss)
{
	(static_cast<K*>(ss->callbackData_)->*cb)(cs, ss);
}
// }}}

} // namespace x0

#endif
