// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/io/Pipe.h>
#include <base/Socket.h>
#include <base/sysconfig.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

namespace base {

/** Creates a kernel pipe.
 *
 * @param flags an OR'ed value of O_NONBLOCK and O_CLOEXEC
 */
Pipe::Pipe(int flags) : size_(0) {
#if defined(HAVE_PIPE2)
  if (::pipe2(pipe_, flags) < 0) {
    pipe_[0] = -errno;
    pipe_[1] = -1;
  }
#else
  if (::pipe(pipe_) < 0) {
    pipe_[0] = -errno;
    pipe_[1] = -1;
  } else {
    fcntl(pipe_[0], F_SETFL, fcntl(pipe_[0], F_GETFL) | flags);
    fcntl(pipe_[1], F_SETFL, fcntl(pipe_[1], F_GETFL) | flags);
  }
#endif
}

/**
 * Consumes all data currently available for read.
 */
void Pipe::clear() {
  char buf[4096];
  ssize_t rv;

  do {
    rv = ::read(readFd(), buf, sizeof(buf));
  } while (rv > 0);

  size_ = 0;
}

/**
 * Writes given buffer into the pipe.
 *
 * @param buf the buffer source to read from
 * @param size number of bytes to write
 * @return number of bytes written or -1 with errno being set.
 */
ssize_t Pipe::write(const void* buf, size_t size) {
  ssize_t rv = ::write(writeFd(), buf, size);

  if (rv > 0) {
    size_ += rv;
  }

  return rv;
}

/**
 * Transfers contens from given socket into this pipe.
 *
 * @param socket the socket source to retrieve the data from
 * @param size number of bytes to consume
 *
 * @return number of bytes consumed from the socket and written into the pipe
 *         or -1 and errno set.
 *
 * @see Socket::read(Pipe* p, size_t n)
 */
ssize_t Pipe::write(Socket* socket, size_t size) {
  return socket->read(this, size);
}

/**
 * Transfers data from given pipe into this pipe.
 *
 * @param pipe the source pipe to transfer the data from.
 * @param size number of bytes to transfer
 *
 * @return number of bytes consumed from the other pipe
 *         and written into this pipe or -1 and errno set.
 */
ssize_t Pipe::write(Pipe* pipe, size_t size) {
  ssize_t rv = 0;

#ifdef __APPLE__
  assert(!"TODO");
#else
  rv = splice(pipe->readFd(), NULL, writeFd(), NULL, pipe->size_,
              SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
#endif

  if (rv > 0) {
    pipe->size_ -= rv;
    size_ += rv;
  }

  return rv;
}

ssize_t Pipe::write(int fd, off_t* fd_off, size_t size) {
  ssize_t rv = 0;

#ifdef __APPLE__
  assert(!"TODO");
#else
  rv = splice(fd, fd_off, writeFd(), NULL, size,
              SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
#endif

  if (rv > 0) size_ += rv;

  return rv;
}

/**
 * Copies the data in @p source into this pipe.
 *
 * @param source the source pipe to copy the data from.
 *               The contents of this pipe will not be consumed.
 * @param size   number of bytes to copy
 *
 * @return number of actual buytes copied or -1 on error and errno is set.
 */
ssize_t Pipe::copy(Pipe* source, ssize_t size) {
#if defined(HAVE_TEE)
  ssize_t rv = tee(source->readFd(), writeFd(), size, 0);
  if (rv > 0) {
    size_ += rv;
  }
  return rv;
#else
  errno = ENOSYS;
  return -1;
#endif
}

ssize_t Pipe::read(void* buf, size_t size) {
  ssize_t rv = ::read(readFd(), buf, size);

  if (rv > 0) size_ -= rv;

  return rv;
}

ssize_t Pipe::read(Socket* socket, size_t size) {
  return socket->read(this, size);
}

ssize_t Pipe::read(Pipe* pipe, size_t size) { return pipe->write(this, size); }

ssize_t Pipe::read(int fd, off_t* fd_off, size_t size) {
  ssize_t rv = 0;

#ifdef __APPLE__
  assert(!"TODO");
#else
  rv = splice(readFd(), fd_off, fd, NULL, size,
              SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
#endif

  if (rv > 0) size_ -= rv;

  return rv;
}

}  // namespace base
