// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/SslEndPoint.h>
#include <xzero/net/SslContext.h>
#include <xzero/net/SslConnector.h>
#include <xzero/net/SslUtil.h>
#include <xzero/net/Connection.h>
#include <xzero/io/FileUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>

namespace xzero {

#ifndef NDEBUG
#define TRACE(msg...) logTrace("SslEndPoint", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

#define THROW_SSL_ERROR() {                                                   \
  RAISE_CATEGORY(ERR_get_error(), SslErrorCategory::get());                      \
}

SslEndPoint::SslEndPoint(FileDescriptor&& fd,
                         Duration readTimeout,
                         Duration writeTimeout,
                         SslContext* defaultContext,
                         std::function<void(EndPoint*)> onEndPointClosed,
                         Executor* executor)
    : handle_(fd.release()),
      isCorking_(false),
      onEndPointClosed_(onEndPointClosed),
      executor_(executor),
      ssl_(nullptr),
      bioDesire_(Desire::None),
      io_(),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      idleTimeout_(executor) {
  TRACE("$0 SslEndPoint() ctor", this);

  idleTimeout_.setCallback(std::bind(&SslEndPoint::onTimeout, this));

  ssl_ = SSL_new(defaultContext->get());
  SSL_set_fd(ssl_, fd);

#if !defined(NDEBUG)
  SSL_set_tlsext_debug_callback(ssl_, &SslEndPoint::tlsext_debug_cb);
  SSL_set_tlsext_debug_arg(ssl_, this);
#endif
}

SslEndPoint::~SslEndPoint() {
  TRACE("$0 ~SslEndPoint() dtor", this);

  SSL_free(ssl_);
  FileUtil::close(handle());
}

bool SslEndPoint::isOpen() const {
  return SSL_get_shutdown(ssl_) == 0;
}

void SslEndPoint::close() {
  if (!isOpen())
    return;

  shutdown();
}

void SslEndPoint::shutdown() {
  int rv = SSL_shutdown(ssl_);

  TRACE("$0 close: SSL_shutdown -> $1", this, rv);
  if (rv == 1) {
    onEndPointClosed_(this);
  } else if (rv == 0) {
    // call again
    shutdown();
  } else {
    switch (SSL_get_error(ssl_, rv)) {
      case SSL_ERROR_WANT_READ:
        io_ = executor_->executeOnReadable(
            handle(),
            std::bind(&SslEndPoint::shutdown, this));
        break;
      case SSL_ERROR_WANT_WRITE:
        io_ = executor_->executeOnWritable(
            handle(),
            std::bind(&SslEndPoint::shutdown, this));
        break;
      default:
        THROW_SSL_ERROR();
    }
  }
}

void SslEndPoint::abort() {
#if 0
  // pretend we did a full shutdown
  SSL_set_shutdown(ssl_, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
  shutdown();
#else
  onEndPointClosed_(this);
#endif
}

size_t SslEndPoint::fill(Buffer* sink, size_t space) {
  int rv = SSL_read(ssl_, sink->end(), space);
  if (rv > 0) {
    TRACE("$0 fill(Buffer:$1) -> $2", this, space, rv);
    bioDesire_ = Desire::None;
    sink->resize(sink->size() + rv);
    return rv;
  }

  switch (SSL_get_error(ssl_, rv)) {
    case SSL_ERROR_WANT_READ:
      TRACE("$0 fill(Buffer:$1) -> want read", this, space);
      bioDesire_ = Desire::Read;
      break;
    case SSL_ERROR_WANT_WRITE:
      TRACE("$0 fill(Buffer:$1) -> want write", this, space);
      bioDesire_ = Desire::Write;
      break;
    case SSL_ERROR_ZERO_RETURN:
      TRACE("$0 fill(Buffer:$1) -> remote endpoint closed", this, space);
      abort();
      break;
    default:
      TRACE("$0 fill(Buffer:$1): SSL_read() -> $2",
            this, space, SSL_get_error(ssl_, rv));
      THROW_SSL_ERROR();
  }
  errno = EAGAIN;
  return 0;
}

size_t SslEndPoint::flush(const BufferRef& source) {
  int rv = SSL_write(ssl_, source.data(), source.size());
  if (rv > 0) {
    bioDesire_ = Desire::None;
    TRACE("$0 flush(BufferRef, $1, $2/$3 bytes)",
          this, source.data(), rv, source.size());

    return rv;
  }

  switch (SSL_get_error(ssl_, rv)) {
    case SSL_ERROR_WANT_READ:
      TRACE("$0 flush(BufferRef, @$1, $2 bytes) failed -> want read.", this, source.data(), source.size());
      bioDesire_ = Desire::Read;
      break;
    case SSL_ERROR_WANT_WRITE:
      TRACE("$0 flush(BufferRef, @$1, $2 bytes) failed -> want write.", this, source.data(), source.size());
      bioDesire_ = Desire::Read;
      break;
    case SSL_ERROR_ZERO_RETURN:
      TRACE("$0 flush(BufferRef, @$1, $2 bytes) failed -> remote endpoint closed.", this, source.data(), source.size());
      abort();
      break;
    default:
      TRACE("$0 flush(BufferRef, @$1, $2 bytes) failed. error.", this, source.data(), source.size());
      THROW_SSL_ERROR();
  }
  errno = EAGAIN;
  return 0;
}

size_t SslEndPoint::flush(int fd, off_t offset, size_t size) {
  Buffer buf;
  buf.reserve(size);
  ssize_t rv = ::read(fd, buf.data(), buf.capacity());
  if (rv < 0) {
    switch (errno) {
      case EBUSY:
      case EAGAIN:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
      case EWOULDBLOCK:
#endif
        return 0;
      default:
        RAISE_ERRNO(errno);
    }
  }

  buf.resize(static_cast<size_t>(rv));
  return flush(buf);
}

void SslEndPoint::wantFill() {
  if (io_) {
    TRACE("$0 wantFill: ignored due to active io", this);
    return;
  }

  switch (bioDesire_) {
    case Desire::None:
    case Desire::Read:
      TRACE("$0 wantFill: read", this);
      io_ = executor_->executeOnReadable(
          handle(),
          std::bind(&SslEndPoint::fillable, this));
      break;
    case Desire::Write:
      TRACE("$0 wantFill: write", this);
      io_ = executor_->executeOnWritable(
          handle(),
          std::bind(&SslEndPoint::fillable, this));
      break;
  }
}

void SslEndPoint::fillable() {
  TRACE("$0 fillable()", this);
  RefPtr<EndPoint> _guard(this);
  try {
    io_.reset();
    bioDesire_ = Desire::None;
    connection()->onFillable();
  } catch (const std::exception& e) {
    connection()->onInterestFailure(e);
  } catch (...) {
    connection()->onInterestFailure(
        EXCEPTION(RuntimeError, (int) Status::CaughtUnknownExceptionError,
                  StatusCategory::get()));
  }
}

void SslEndPoint::wantFlush() {
  if (io_) {
    TRACE("$0 wantFlush: ignored due to active io", this);
    return;
  }

  switch (bioDesire_) {
    case Desire::Read:
      TRACE("$0 wantFlush: read", this);
      io_ = executor_->executeOnReadable(
          handle(),
          std::bind(&SslEndPoint::flushable, this));
      break;
    case Desire::None:
    case Desire::Write:
      TRACE("$0 wantFlush: write", this);
      io_ = executor_->executeOnWritable(
          handle(),
          std::bind(&SslEndPoint::flushable, this));
      break;
  }
}

Duration SslEndPoint::readTimeout() {
  return readTimeout_;
}

Duration SslEndPoint::writeTimeout() {
  return writeTimeout_;
}

void SslEndPoint::setReadTimeout(Duration timeout) {
  readTimeout_ = timeout;
}

void SslEndPoint::setWriteTimeout(Duration timeout) {
  writeTimeout_ = timeout;
}

bool SslEndPoint::isBlocking() const {
  return !(fcntl(handle(), F_GETFL) & O_NONBLOCK);
}

void SslEndPoint::setBlocking(bool enable) {
  TRACE("$0 setBlocking($1)", this, enable);
#if 1
  unsigned current = fcntl(handle(), F_GETFL);
  unsigned flags = enable ? (current & ~O_NONBLOCK)
                          : (current | O_NONBLOCK);
#else
  unsigned flags = enable ? fcntl(handle(), F_GETFL) & ~O_NONBLOCK
                          : fcntl(handle(), F_GETFL) | O_NONBLOCK;
#endif

  if (fcntl(handle(), F_SETFL, flags) < 0) {
    RAISE_ERRNO(errno);
  }
}

bool SslEndPoint::isCorking() const {
  return isCorking_;
}

void SslEndPoint::setCorking(bool enable) {
#if defined(TCP_CORK)
  if (isCorking_ != enable) {
    int flag = enable ? 1 : 0;
    if (setsockopt(handle(), IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag)) < 0)
      RAISE_ERRNO(errno);

    isCorking_ = enable;
  }
#endif
}

bool SslEndPoint::isTcpNoDelay() const {
  int result = 0;
  socklen_t sz = sizeof(result);
  if (getsockopt(handle_, IPPROTO_TCP, TCP_NODELAY, &result, &sz) < 0)
    RAISE_ERRNO(errno);

  return result;
}

void SslEndPoint::setTcpNoDelay(bool enable) {
  int flag = enable ? 1 : 0;
  if (setsockopt(handle_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0)
    RAISE_ERRNO(errno);
}

std::string SslEndPoint::toString() const {
  char buf[64];
  int n = snprintf(buf, sizeof(buf), "SslEndPoint(fd=%d)", handle());
  return std::string(buf, n);
}

void SslEndPoint::onHandshake() {
  TRACE("$0 onHandshake begin...", this);
  int rv = SSL_accept(ssl_);
  if (rv <= 0) {
    switch (SSL_get_error(ssl_, rv)) {
      case SSL_ERROR_WANT_READ:
        TRACE("$0 onHandshake (want read)", this);
        executor_->executeOnReadable(
            handle(), std::bind(&SslEndPoint::onHandshake, this));
        break;
      case SSL_ERROR_WANT_WRITE:
        TRACE("$0 onHandshake (want write)", this);
        executor_->executeOnWritable(
            handle(), std::bind(&SslEndPoint::onHandshake, this));
        break;
      default: {
        TRACE("$0 onHandshake (error)", this);
        char buf[256];
        ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
        RAISE_ERRNO(errno);
      }
    }
  } else {
    // create associated Connection object and run it
    bioDesire_ = Desire::None;
    RefPtr<EndPoint> _guard(this);
    TRACE("$0 handshake complete (next protocol: \"$1\")", this, nextProtocolNegotiated().str().c_str());

    std::string protocol = nextProtocolNegotiated().str();
    // TODO FIXME getting connectionFactory_
    // std::unique_ptr<Connection> conn(connectionFactory_(protocol));
    // setConnection(std::move(conn));

    connection()->onOpen(false);
  }
}

void SslEndPoint::flushable() {
  TRACE("$0 flushable()", this);
  RefPtr<EndPoint> _guard(this);
  try {
    io_.reset();
    bioDesire_ = Desire::None;
    connection()->onFlushable();
  } catch (const std::exception& e) {
    connection()->onInterestFailure(e);
  } catch (...) {
    connection()->onInterestFailure(
        EXCEPTION(RuntimeError, (int) Status::CaughtUnknownExceptionError,
                  StatusCategory::get()));
  }
}

void SslEndPoint::onTimeout() {
  if (connection()) {
    bool finalize = connection()->onReadTimeout();

    if (finalize) {
      this->abort();
    }
  }
}

BufferRef SslEndPoint::nextProtocolNegotiated() const {
  const unsigned char* data = nullptr;
  unsigned int len = 0;

#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation // ALPN
  SSL_get0_alpn_selected(ssl_, &data, &len);
  if (len > 0) {
    return BufferRef((const char*) data, len);
  }
#endif

#ifdef TLSEXT_TYPE_next_proto_neg // NPN
  SSL_get0_next_proto_negotiated(ssl_, &data, &len);
  if (len > 0) {
    return BufferRef((const char*) data, len);
  }
#endif

  return BufferRef();
}

#if !defined(NDEBUG)
static inline std::string tlsext_type_to_string(int type) {
  switch (type) {
    case TLSEXT_TYPE_server_name: return "server name";
    case TLSEXT_TYPE_max_fragment_length: return "max fragment length";
    case TLSEXT_TYPE_client_certificate_url: return "client certificate url";
    case TLSEXT_TYPE_trusted_ca_keys: return "trusted ca keys";
    case TLSEXT_TYPE_truncated_hmac: return "truncated hmac";
    case TLSEXT_TYPE_status_request: return "status request";
#if defined(TLSEXT_TYPE_user_mapping)
    case TLSEXT_TYPE_user_mapping: return "user mapping";
#endif
#if defined(TLSEXT_TYPE_client_authz)
    case TLSEXT_TYPE_client_authz: return "client authz";
#endif
#if defined(TLSEXT_TYPE_server_authz)
    case TLSEXT_TYPE_server_authz: return "server authz";
#endif
#if defined(TLSEXT_TYPE_cert_type)
    case TLSEXT_TYPE_cert_type: return "cert type";
#endif
    case TLSEXT_TYPE_elliptic_curves: return "elliptic curves";
    case TLSEXT_TYPE_ec_point_formats: return "EC point formats";
#if defined(TLSEXT_TYPE_srp)
    case TLSEXT_TYPE_srp: return "SRP";
#endif
#if defined(TLSEXT_TYPE_signature_algorithms)
    case TLSEXT_TYPE_signature_algorithms: return "signature algorithms";
#endif
#if defined(TLSEXT_TYPE_use_srtp)
    case TLSEXT_TYPE_use_srtp: return "use SRTP";
#endif
#if defined(TLSEXT_TYPE_heartbeat)
    case TLSEXT_TYPE_heartbeat: return "heartbeat";
#endif
    case TLSEXT_TYPE_session_ticket: return "session ticket";
    case TLSEXT_TYPE_renegotiate: return "renegotiate";
#if defined(TLSEXT_TYPE_opaque_prf_input)
    case TLSEXT_TYPE_opaque_prf_input: return "opaque PRF input";
#endif
#if defined(TLSEXT_TYPE_next_proto_neg) // NPN
    case TLSEXT_TYPE_next_proto_neg: return "next protocol negotiation";
#endif
#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation // ALPN
    case TLSEXT_TYPE_application_layer_protocol_negotiation: return "Application Layer Protocol Negotiation";
#endif
#if defined(TLSEXT_TYPE_padding)
    case TLSEXT_TYPE_padding: return "padding";
#endif
    default: return "UNKNOWN";
  }
}

void SslEndPoint::tlsext_debug_cb(
    SSL* ssl, int client_server, int type,
    unsigned char* data, int len, SslEndPoint* self) {
  TRACE("$0 TLS $1 extension \"$2\" (id=$3), len=$4",
        self,
        client_server ? "server" : "client",
        tlsext_type_to_string(type),
        type,
        len);
}
#endif // !NDEBUG

template<> std::string StringUtil::toString(SslEndPoint* ep) {
  return StringUtil::format("SslEndPoint/$0", (void*) ep);
}

} // namespace xzero
