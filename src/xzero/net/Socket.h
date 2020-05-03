// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/Result.h>
#include <xzero/defines.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/InetAddress.h>
#include <xzero/sysconfig.h>

#include <fmt/format.h>

#if defined(XZERO_OS_WINDOWS)
#include <winsock2.h>
#endif

namespace xzero {

class [[nodiscard]] Socket {
 public:
  using AddressFamily = IPAddress::Family;

 private:
  enum Type { TCP, UDP };
  enum BlockingMode { Blocking, NonBlocking };

  Socket(const Socket& other) = delete;
  Socket& operator=(const Socket& other) = delete;
  Socket(AddressFamily family, Type type, BlockingMode bm);

#if defined(XZERO_OS_UNIX)
  Socket(AddressFamily family, FileDescriptor&& fd);
#endif

#if defined(XZERO_OS_WINDOWS)
  Socket(AddressFamily family, SOCKET s);
#endif

 public:
  ~Socket();

  enum InvalidSocketState { InvalidSocket };
  explicit Socket(InvalidSocketState);

  enum NonBlockingTcpCtor { NonBlockingTCP };
  explicit Socket(NonBlockingTcpCtor, AddressFamily af = IPAddress::Family::V6);

  Socket(Socket&& other);
  Socket& operator=(Socket&& other);

  static Socket make_tcp_ip(bool nonBlocking, AddressFamily = AddressFamily::V6);
  static Socket make_udp_ip(bool nonBlocking, AddressFamily = AddressFamily::V6);

#if defined(XZERO_OS_UNIX)
  static Socket make_socket(AddressFamily af, FileDescriptor&& fd);
#endif

#if defined(XZERO_OS_WINDOWS)
  static Socket make_socket(AddressFamily af, SOCKET s);
#endif

  bool valid() const noexcept;
  void close();

  int getLocalPort() const;
  [[nodiscard]] Result<InetAddress> getLocalAddress() const;
  [[nodiscard]] Result<InetAddress> getRemoteAddress() const;

  AddressFamily addressFamily() const noexcept { return addressFamily_; }

  int write(const void* buf, size_t count);
  void consume();

  void setBlocking(bool enable);

  std::error_code connect(const InetAddress& address);

#if defined(XZERO_OS_UNIX)
  operator int () const noexcept { return handle_; }
  int native() const noexcept { return handle_; }
  int release() { return handle_.release(); }
#endif

#if defined(XZERO_OS_WINDOWS)
  operator SOCKET () const noexcept { return handle_; }
  SOCKET native() const noexcept { return handle_; }
  SOCKET release() { SOCKET t = handle_; close(); return t; }
#endif

  void swap(Socket& other) {
    std::swap(handle_, other.handle_);
    std::swap(addressFamily_, other.addressFamily_);
  }

 private:
#if defined(XZERO_OS_UNIX)
  FileDescriptor handle_;
#endif

#if defined(XZERO_OS_WINDOWS)
  SOCKET handle_;
#endif

  AddressFamily addressFamily_;
};

inline void swap(Socket& a, Socket& b) {
  a.swap(b);
}

} // namespace xzero

namespace fmt {
  template<>
  struct formatter<xzero::Socket> {
    using Socket = xzero::Socket;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const Socket& v, FormatContext &ctx) {
#if defined(XZERO_OS_UNIX)
      return format_to(ctx.out(), "{}", v.native());
#else
      return format_to(ctx.out(), /* TODO */);
#endif
    }
  };
}
