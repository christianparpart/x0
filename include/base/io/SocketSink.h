// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_SocketSink_h
#define sw_x0_io_SocketSink_h 1

#include <base/io/Sink.h>
#include <base/Socket.h>
#include <base/Types.h>

namespace base {

//! \addtogroup io
//@{

/** file descriptor stream sink.
 */
class BASE_API SocketSink : public Sink {
 public:
  explicit SocketSink(Socket *conn);

  Socket *socket() const;
  void setSocket(Socket *);

  virtual void accept(SinkVisitor &v);
  virtual ssize_t write(const void *buffer, size_t size);
  ssize_t write(int fd, off_t *offset, size_t nbytes);
  ssize_t write(Pipe *pipe, size_t size);

 protected:
  Socket *socket_;
};

//@}

// {{{ impl
inline Socket *SocketSink::socket() const { return socket_; }

inline void SocketSink::setSocket(Socket *value) { socket_ = value; }

inline ssize_t SocketSink::write(int fd, off_t *offset, size_t nbytes) {
  return socket_->write(fd, offset, nbytes);
}

inline ssize_t SocketSink::write(Pipe *pipe, size_t size) {
  return socket_->write(pipe, size);
}
// }}}

}  // namespace base

#endif
