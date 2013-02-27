/* <Pipe.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_Pipe_h
#define sw_x0_Pipe_h (1)

#include <x0/Api.h>
#include <unistd.h>

namespace x0 {

class Socket;

//! \addtogroup io
//@{

class X0_API Pipe
{
private:
	int pipe_[2];
	size_t size_; // number of bytes available in pipe

private:
	int writeFd() const;
	int readFd() const;

public:
	explicit Pipe(int flags = 0);
	~Pipe();

	bool isOpen() const;

	size_t size() const;
	bool isEmpty() const;

	void clear();

	// write to pipe
	ssize_t write(const void* buf, size_t size);
	ssize_t write(Socket* socket, size_t size);
	ssize_t write(Pipe* pipe, size_t size);
	ssize_t write(int fd, size_t size);
	ssize_t write(int fd, off_t *fd_off, size_t size);

	// read from pipe
	ssize_t read(void* buf, size_t size);
	ssize_t read(Socket* socket, size_t size);
	ssize_t read(Pipe* socket, size_t size);
	ssize_t read(int fd, size_t size);
	ssize_t read(int fd, off_t *fd_off, size_t size);
};

//@}

// {{{ impl
inline Pipe::~Pipe()
{
	if (isOpen()) {
		::close(pipe_[0]);
		::close(pipe_[1]);
	}
}

inline bool Pipe::isOpen() const
{
	return pipe_[0] >= 0;
}

inline int Pipe::writeFd() const
{
	return pipe_[1];
}

inline int Pipe::readFd() const
{
	return pipe_[0];
}

inline size_t Pipe::size() const
{
	return size_;
}

inline bool Pipe::isEmpty() const
{
	return size_ == 0;
}

inline ssize_t Pipe::write(int fd, size_t size)
{
	return write(fd, NULL, size);
}

inline ssize_t Pipe::read(int fd, size_t size)
{
	return read(fd, NULL, size);
}
// }}}

} // namespace x0

#endif
