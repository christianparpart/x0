// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/net/InetEndPoint.h>
#include <xzero/net/InetConnector.h>
#include <xzero/net/Connection.h>
#include <xzero/executor/Executor.h>
#include <xzero/logging.h>
#include <xzero/RuntimeError.h>
#include <xzero/Buffer.h>
#include <xzero/sysconfig.h>
#include <xzero/RefPtr.h>
#include <stdexcept>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#if defined(HAVE_SYS_SENDFILE_H)
#include <sys/sendfile.h>
#endif

namespace xzero {

#define ERROR(msg...) logError("net.InetEndPoint", msg)

#if 0 // !defined(NDEBUG)
#define TRACE(msg...) logTrace("net.InetEndPoint", msg)
#define DEBUG(msg...) logDebug("net.InetEndPoint", msg)
#else
#define TRACE(msg...) do {} while (0)
#define DEBUG(msg...) do {} while (0)
#endif

InetEndPoint::InetEndPoint(int socket,
                           InetConnector* connector,
                           Executor* executor)
    : EndPoint(),
      connector_(connector),
      executor_(executor),
      readTimeout_(connector->readTimeout()),
      writeTimeout_(connector->writeTimeout()),
      io_(),
      handle_(socket),
      addressFamily_(connector->addressFamily()),
      isCorking_(false) {
  TRACE("$0 ctor fd=$1", this, handle_);
}

InetEndPoint::InetEndPoint(int socket,
                           int addressFamily,
                           Duration readTimeout,
                           Duration writeTimeout,
                           Executor* executor)
    : EndPoint(),
      connector_(nullptr),
      executor_(executor),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      io_(),
      handle_(socket),
      addressFamily_(addressFamily),
      isCorking_(false) {
  TRACE("$0 ctor", this);
}

void InetEndPoint::onTimeout() {
  if (connection()) {
    if (connection()->onReadTimeout()) {
      close();
    }
  }
}

InetEndPoint::~InetEndPoint() {
  TRACE("$0 dtor", this);
  if (isOpen()) {
    close();
  }
}

Option<std::pair<IPAddress, int>> InetEndPoint::remoteAddress() const {
  if (handle_ < 0)
    return None();

  std::pair<IPAddress, int> result;
  switch (addressFamily()) {
    case AF_INET6: {
      sockaddr_in6 saddr;
      socklen_t slen = sizeof(saddr);
      if (getpeername(handle_, (sockaddr*)&saddr, &slen) < 0)
        return None();

      result.first = IPAddress(&saddr);
      result.second = ntohs(saddr.sin6_port);
      break;
    }
    case AF_INET: {
      sockaddr_in saddr;
      socklen_t slen = sizeof(saddr);
      if (getpeername(handle_, (sockaddr*)&saddr, &slen) < 0)
        return None();

      result.first = IPAddress(&saddr);
      result.second = ntohs(saddr.sin_port);
      break;
    }
    default:
      RAISE(IllegalStateError, "Invalid address family.");
  }
  return Some<std::pair<IPAddress, int>>(result);
}

Option<std::pair<IPAddress, int>> InetEndPoint::localAddress() const {
  if (handle_ < 0)
    return None();

  std::pair<IPAddress, int> result;
  switch (addressFamily()) {
    case AF_INET6: {
      sockaddr_in6 saddr;
      socklen_t slen = sizeof(saddr);

      if (getsockname(handle_, (sockaddr*)&saddr, &slen) == 0) {
        result.first = IPAddress(&saddr);
        result.second = ntohs(saddr.sin6_port);
      }
      break;
    }
    case AF_INET: {
      sockaddr_in saddr;
      socklen_t slen = sizeof(saddr);

      if (getsockname(handle_, (sockaddr*)&saddr, &slen) == 0) {
        result.first = IPAddress(&saddr);
        result.second = ntohs(saddr.sin_port);
      }
      break;
    }
    default:
      break;
  }

  return Some<std::pair<IPAddress, int>>(result);
}

bool InetEndPoint::isOpen() const XZERO_NOEXCEPT {
  return handle_ >= 0;
}

void InetEndPoint::close() {
  if (isOpen()) {
    TRACE("close() fd=$0", handle_);
    ::close(handle_);
    handle_ = -1;

    if (connector_) {
      connector_->onEndPointClosed(this);
    }
  }
}

bool InetEndPoint::isBlocking() const {
  return !(fcntl(handle_, F_GETFL) & O_NONBLOCK);
}

void InetEndPoint::setBlocking(bool enable) {
  TRACE("$0 setBlocking(\"$1\")", this, enable);
#if 1
  unsigned current = fcntl(handle_, F_GETFL);
  unsigned flags = enable ? (current & ~O_NONBLOCK)
                          : (current | O_NONBLOCK);
#else
  unsigned flags = enable ? fcntl(handle_, F_GETFL) & ~O_NONBLOCK
                          : fcntl(handle_, F_GETFL) | O_NONBLOCK;
#endif

  if (fcntl(handle_, F_SETFL, flags) < 0) {
    RAISE_ERRNO(errno);
  }
}

bool InetEndPoint::isCorking() const {
  return isCorking_;
}

void InetEndPoint::setCorking(bool enable) {
#if defined(TCP_CORK)
  if (isCorking_ != enable) {
    int flag = enable ? 1 : 0;
    if (setsockopt(handle_, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag)) < 0)
      RAISE_ERRNO(errno);

    isCorking_ = enable;
  }
#endif
}

bool InetEndPoint::isTcpNoDelay() const {
  int result = 0;
  socklen_t sz = sizeof(result);
  if (getsockopt(handle_, IPPROTO_TCP, TCP_NODELAY, &result, &sz) < 0)
    RAISE_ERRNO(errno);

  return result;
}

void InetEndPoint::setTcpNoDelay(bool enable) {
  int flag = enable ? 1 : 0;
  if (setsockopt(handle_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0)
    RAISE_ERRNO(errno);
}

std::string InetEndPoint::toString() const {
  char buf[32];
  snprintf(buf, sizeof(buf), "InetEndPoint(%d)@%p", handle(), this);
  return buf;
}

size_t InetEndPoint::fill(Buffer* result) {
  result->reserve(result->size() + 1024);
  ssize_t n = read(handle(), result->end(), result->capacity() - result->size());
  TRACE("read($0 bytes) -> $1", result->capacity() - result->size(), n);

  if (n < 0) {
    // don't raise on soft errors, such as there is simply no more data to read.
    switch (errno) {
      case EBUSY:
      case EAGAIN:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
      case EWOULDBLOCK:
#endif
        break;
      default:
        RAISE_ERRNO(errno);
    }
  } else {
    result->resize(result->size() + n);
  }

  return n;
}

size_t InetEndPoint::flush(const BufferRef& source) {
  ssize_t rv = write(handle(), source.data(), source.size());

  TRACE("flush($0 bytes) -> $1", source.size(), rv);

  if (rv < 0)
    RAISE_ERRNO(errno);

  // EOF exception?

  return rv;
}

size_t InetEndPoint::flush(int fd, off_t offset, size_t size) {
#if defined(__APPLE__)
  off_t len = 0;
  int rv = sendfile(fd, handle(), offset, &len, nullptr, 0);
  TRACE("flush(offset:$0, size:$1) -> $2", offset, size, rv);
  if (rv < 0)
    RAISE_ERRNO(errno);

  return len;
#else
  ssize_t rv = sendfile(handle(), fd, &offset, size);
  TRACE("flush(offset:$0, size:$1) -> $2", offset, size, rv);
  if (rv < 0)
    RAISE_ERRNO(errno);

  // EOF exception?

  return rv;
#endif
}

void InetEndPoint::wantFill() {
  TRACE("$0 wantFill()", this);
  // TODO: abstract away the logic of TCP_DEFER_ACCEPT

  if (!io_) {
    io_ = executor_->executeOnReadable(
        handle(),
        std::bind(&InetEndPoint::fillable, this),
        readTimeout(),
        std::bind(&InetEndPoint::onTimeout, this));
  }
}

void InetEndPoint::fillable() {
  RefPtr<EndPoint> _guard(this);

  try {
    io_.reset();
    connection()->onFillable();
  } catch (const std::exception& e) {
    connection()->onInterestFailure(e);
  } catch (...) {
    connection()->onInterestFailure(
        EXCEPTION(RuntimeError, (int) Status::CaughtUnknownExceptionError,
                  StatusCategory::get()));
  }
}

void InetEndPoint::wantFlush() {
  TRACE("$0 wantFlush() $1", this, io_.get() ? "again" : "first time");

  if (!io_) {
    io_ = executor_->executeOnWritable(
        handle(),
        std::bind(&InetEndPoint::flushable, this),
        writeTimeout(),
        std::bind(&InetEndPoint::onTimeout, this));
  }
}

void InetEndPoint::flushable() {
  RefPtr<EndPoint> _guard(this);

  try {
    io_.reset();
    connection()->onFlushable();
  } catch (const std::exception& e) {
    connection()->onInterestFailure(e);
  } catch (...) {
    connection()->onInterestFailure(
        EXCEPTION(RuntimeError, (int) Status::CaughtUnknownExceptionError,
                  StatusCategory::get()));
  }
}

Duration InetEndPoint::readTimeout() {
  return readTimeout_;
}

Duration InetEndPoint::writeTimeout() {
  return writeTimeout_;
}

void InetEndPoint::setReadTimeout(Duration timeout) {
  readTimeout_ = timeout;
}

void InetEndPoint::setWriteTimeout(Duration timeout) {
  writeTimeout_ = timeout;
}

Option<IPAddress> InetEndPoint::remoteIP() const {
  auto remote = remoteAddress();
  if (remote.isEmpty())
    return None();
  else
    return remote.get().first;
}

class InetConnectState {
 public:
  IPAddress ipaddr_;
  int port_;
  RefPtr<InetEndPoint> ep_;
  Promise<RefPtr<EndPoint>> promise_;

  InetConnectState(const IPAddress& ipaddr, int port,
                   RefPtr<InetEndPoint> ep,
                   const Promise<RefPtr<EndPoint>>& promise)
      : ipaddr_(ipaddr), port_(port), ep_(std::move(ep)), promise_(promise) {
  }

  void onConnectComplete() {
    int val = 0;
    socklen_t vlen = sizeof(val);
    if (getsockopt(ep_->handle(), SOL_SOCKET, SO_ERROR, &val, &vlen) == 0) {
      if (val == 0) {
        TRACE("$0 onConnectComplete: connected $1. $2", this, val, strerror(val));
        promise_.success(ep_.as<EndPoint>());
      } else {
        DEBUG("Connecting to $0:$1 failed. $2", ipaddr_, port_, strerror(val));
        promise_.failure(Status::IOError); // dislike: wanna pass errno val here.
      }
    } else {
      DEBUG("Connecting to $0:$1 failed. $2", ipaddr_, port_, strerror(val));
      promise_.failure(Status::IOError); // dislike: wanna pass errno val here.
    }
    delete this;
  }
};

template<>
std::string StringUtil::toString(InetConnectState* obj) {
  return StringUtil::format("InetConnectState[$0]", (void*) obj);
}

Future<RefPtr<EndPoint>> InetEndPoint::connectAsync(const IPAddress& ipaddr,
                                                    int port,
                                                    Duration connectTimeout,
                                                    Duration readTimeout,
                                                    Duration writeTimeout,
                                                    Executor* executor) {
  int fd = socket(ipaddr.family(), SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0)
    RAISE_ERRNO(errno);

  Promise<RefPtr<EndPoint>> promise;
  RefPtr<InetEndPoint> ep;

  try {
    TRACE("connectAsync: to $0 port $1", ipaddr, port);
    ep = new InetEndPoint(fd, ipaddr.family(), readTimeout, writeTimeout,
                          executor);
    ep->setBlocking(false);

    switch (ipaddr.family()) {
      case AF_INET: {
        struct sockaddr_in saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = ipaddr.family();
        saddr.sin_port = htons(port);
        memcpy(&saddr.sin_addr, ipaddr.data(), ipaddr.size());

        // this connect()-call can block if fd is non-blocking
        TRACE("connectAsync: connect(ipv4)");
        if (::connect(fd, (const struct sockaddr*) &saddr, sizeof(saddr)) < 0) {
          if (errno != EINPROGRESS) {
            TRACE("connectAsync: connect() error. $0", strerror(errno));
            promise.failure(Status::IOError); // errno
            return promise.future();
          } else {
            TRACE("connectAsync: backgrounding");
            executor->executeOnWritable(fd,
                std::bind(&InetConnectState::onConnectComplete,
                          new InetConnectState(ipaddr, port, ep,
                                               promise)));
            return promise.future();
          }
        }
        TRACE("connectAsync: synchronous connect");
        break;
      }
      case AF_INET6: {
        struct sockaddr_in6 saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin6_family = ipaddr.family();
        saddr.sin6_port = htons(port);
        memcpy(&saddr.sin6_addr, ipaddr.data(), ipaddr.size());

        // this connect()-call can block if fd is non-blocking
        if (::connect(fd, (const struct sockaddr*) &saddr, sizeof(saddr)) < 0) {
          if (errno != EINPROGRESS) {
            TRACE("connectAsync: connect() error. $0", strerror(errno));
            RAISE_ERRNO(errno);
          } else {
            TRACE("connectAsync: backgrounding");
            executor->executeOnWritable(fd,
                std::bind(&InetConnectState::onConnectComplete,
                          new InetConnectState(ipaddr, port, std::move(ep),
                                               promise)));
            return promise.future();
          }
        }
        break;
      }
      default: {
        RAISE(NotImplementedError);
      }
    }

    TRACE("connectAsync: connected instantly");
    promise.success(ep.as<EndPoint>());
    return promise.future();
  } catch (...) {
    ::close(fd);
    throw;
  }
}

void InetEndPoint::connectAsync(
    const IPAddress& ipaddr, int port,
    Duration connectTimeout,
    Duration readTimeout,
    Duration writeTimeout,
    Executor* executor,
    std::function<void(RefPtr<EndPoint>)> onSuccess,
    std::function<void(Status)> onError) {
  Future<RefPtr<EndPoint>> f = connectAsync(
      ipaddr, port, connectTimeout, readTimeout, writeTimeout, executor);

  f.onSuccess([onSuccess] (const RefPtr<EndPoint>& x) {
      onSuccess(const_cast<RefPtr<EndPoint>&>(x));
  });
  f.onFailure(onError);
}

RefPtr<EndPoint> InetEndPoint::connect(
    const IPAddress& ipaddr, int port,
    Duration connectTimeout,
    Duration readTimeout,
    Duration writeTimeout,
    Executor* executor) {
  Future<RefPtr<EndPoint>> f = connectAsync(
      ipaddr, port, connectTimeout, readTimeout, writeTimeout, executor);
  f.wait();

  RefPtr<EndPoint> ep = f.get();

  ep.as<InetEndPoint>()->setBlocking(true);

  return ep;
}

template<>
std::string StringUtil::toString(InetEndPoint* ep) {
  auto addr = ep->remoteAddress();
  if (addr.isSome())
    return StringUtil::format("$0:$1", addr->first, addr->second);
  else
    return "null";
}

} // namespace xzero
