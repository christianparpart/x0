#ifndef sw_x0_io_hpp
#define sw_x0_io_hpp (1)

#include <ev++.h>
#include <system_error>
#include <unistd.h>

namespace x0 {

class Socket;

class SocketDriver
{
private:
	struct ev_loop *loop_;

public:
	explicit SocketDriver(struct ev_loop *loop);
	virtual ~SocketDriver();

	virtual bool isSecure() const;

	virtual Socket *create(int handle);
	virtual void destroy(Socket *);
};

} // namespace x0

#endif
