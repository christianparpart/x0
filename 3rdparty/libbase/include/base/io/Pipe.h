// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_Pipe_h
#define sw_x0_Pipe_h (1)

#include <base/Api.h>
#include <unistd.h>

namespace base {

class Socket;

//! \addtogroup io
//@{

class BASE_API Pipe {
 private:
  int pipe_[2];
  size_t size_;  // number of bytes available in pipe

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
  ssize_t write(int fd, off_t* fd_off, size_t size);

  // read from pipe
  ssize_t read(void* buf, size_t size);
  ssize_t read(Socket* socket, size_t size);
  ssize_t read(Pipe* socket, size_t size);
  ssize_t read(int fd, size_t size);
  ssize_t read(int fd, off_t* fd_off, size_t size);
};

//@}

// {{{ impl
inline Pipe::~Pipe() {
  if (isOpen()) {
    ::close(pipe_[0]);
    ::close(pipe_[1]);
  }
}

inline bool Pipe::isOpen() const { return pipe_[0] >= 0; }

inline int Pipe::writeFd() const { return pipe_[1]; }

inline int Pipe::readFd() const { return pipe_[0]; }

inline size_t Pipe::size() const { return size_; }

inline bool Pipe::isEmpty() const { return size_ == 0; }

inline ssize_t Pipe::write(int fd, size_t size) {
  return write(fd, NULL, size);
}

inline ssize_t Pipe::read(int fd, size_t size) { return read(fd, NULL, size); }
// }}}

}  // namespace base

#endif
