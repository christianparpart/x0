// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/SslEndPoint.h>
#include <xzero/net/SslContext.h>
#include <xzero/net/Connection.h>
#include <xzero/net/TcpUtil.h>
#include <xzero/io/FileUtil.h>
#include <xzero/util/BinaryWriter.h>
#include <xzero/BufferUtil.h>
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

// {{{ SslErrorCategory
const char* SslErrorCategory::name() const noexcept {
  return "ssl";
}

std::string SslErrorCategory::message(int ev) const {
  char buf[256];
  ERR_error_string_n(ev, buf, sizeof(buf));
  return buf;
}

std::error_category& SslErrorCategory::get() {
  static SslErrorCategory ec;
  return ec;
}
// }}}

Future<RefPtr<SslEndPoint>> SslEndPoint::connect(
      const InetAddress& target,
      Duration connectTimeout,
      Duration readTimeout,
      Duration writeTimeout,
      Executor* executor,
      const std::string& sni,
      const std::vector<std::string>& applicationProtocolsSupported,
      ProtocolCallback connectionFactory) {

  Promise<RefPtr<SslEndPoint>> promise;

  Future<int> fd = TcpUtil::connect(target, connectTimeout, executor);
  fd.onFailure(promise);

  fd.onSuccess([=](int& socket) {
    TRACE("connect: fd $0 connected", socket);
    auto endpoint = make_ref<SslEndPoint>(FileDescriptor{socket},
                                          target.family(),
                                          readTimeout,
                                          writeTimeout,
                                          executor,
                                          sni,
                                          applicationProtocolsSupported,
                                          connectionFactory);

    // XXX inc ref to keep it alive during async Ops
    endpoint->ref();

    endpoint->onClientHandshake(promise);
  });

  return promise.future();
}

SslEndPoint::SslEndPoint(FileDescriptor&& fd,
                         int addressFamily,
                         Duration readTimeout,
                         Duration writeTimeout,
                         Executor* executor,
                         const std::string& sni,
                         const std::vector<std::string>& applicationProtocolsSupported,
                         ProtocolCallback connectionFactory)
    : TcpEndPoint(fd.release(), addressFamily,
                  readTimeout, writeTimeout,
                  executor, /*onEndPointClosed*/ nullptr),
      ssl_(nullptr),
      bioDesire_(Desire::None),
      connectionFactory_(connectionFactory)
{
  TRACE("$0 SslEndPoint() ctor, cfd=$1", this, handle());
  SslContext::initialize();

  SSL_CTX* ctx;
#if defined(SSL_TXT_TLSV1_2)
  ctx = SSL_CTX_new(TLSv1_2_client_method());
#elif defined(SSL_TXT_TLSV1_1)
  ctx = SSL_CTX_new(TLSv1_1_client_method());
#else
  ctx = SSL_CTX_new(TLSv1_client_method());
#endif

  // setup ALPN
  Buffer alpn = makeProtocolList(applicationProtocolsSupported);
  SSL_CTX_set_alpn_protos(ctx, (unsigned char*) alpn.data(), alpn.size());

  int verify = SSL_VERIFY_NONE; // or SSL_VERIFY_PEER
  SSL_CTX_set_verify(ctx, verify, &SslEndPoint::onVerifyCallback);

  //.
  ssl_ = SSL_new(ctx);
  SSL_set_fd(ssl_, handle());

  if (!sni.empty()) {
    TRACE("SSL TLS ext host_name: '$0'", sni);
    SSL_set_tlsext_host_name(ssl_, sni.c_str());
  } else {
    TRACE("No SNI provided.");
  }

#if !defined(NDEBUG)
  SSL_set_tlsext_debug_callback(ssl_, &SslEndPoint::tlsext_debug_cb);
  SSL_set_tlsext_debug_arg(ssl_, this);
#endif
}

SslEndPoint::SslEndPoint(FileDescriptor&& fd,
                         int addressFamily,
                         Duration readTimeout,
                         Duration writeTimeout,
                         SslContext* defaultContext,
                         ProtocolCallback connectionFactory,
                         std::function<void(TcpEndPoint*)> onEndPointClosed,
                         Executor* executor)
    : TcpEndPoint(fd.release(), addressFamily,
                  readTimeout, writeTimeout,
                  executor, onEndPointClosed),
      ssl_(nullptr),
      bioDesire_(Desire::None),
      connectionFactory_(connectionFactory) {
  TRACE("$0 SslEndPoint() ctor, cfd=$1", this, handle());

  ssl_ = SSL_new(defaultContext->get());
  SSL_set_fd(ssl_, handle());

#if !defined(NDEBUG)
  SSL_set_tlsext_debug_callback(ssl_, &SslEndPoint::tlsext_debug_cb);
  SSL_set_tlsext_debug_arg(ssl_, this);
#endif
}

SslEndPoint::~SslEndPoint() {
  TRACE("$0 ~SslEndPoint() dtor", this);

  close();
  SSL_free(ssl_);
}

void SslEndPoint::close() {
  if (io_) {
    io_->cancel();
  }
#if 0
  // pretend we did a full shutdown
  SSL_set_shutdown(ssl_, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
  shutdown();
#else
  TcpEndPoint::close();
#endif
  unref(); // XXX
}

void SslEndPoint::shutdown() {
  int rv = SSL_shutdown(ssl_);

  TRACE("$0 close: SSL_shutdown -> $1", this, rv);
  if (rv == 1) {
    TcpEndPoint::close();
    unref(); // XXX
  } else if (rv == 0) {
    // call again
    shutdown();
  } else {
    switch (SSL_get_error(ssl_, rv)) {
      case SSL_ERROR_SYSCALL:
        // consider done
        TcpEndPoint::close();
        unref(); // XXX
        break;
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

size_t SslEndPoint::fill(Buffer* sink, size_t space) {
  int rv = SSL_read(ssl_, sink->end(), space);
  if (rv > 0) {
    TRACE("$0 fill(Buffer:$1) -> $2", this, space, rv);
    bioDesire_ = Desire::None;
    sink->resize(sink->size() + rv);
    return rv;
  }

  switch (SSL_get_error(ssl_, rv)) {
    case SSL_ERROR_SYSCALL:
      if (errno != 0) {
        RAISE_ERRNO(errno);
      } else {
        return 0; // XXX treat as EOF
      }
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
      close();
      break;
    default:
      logDebug("SSL", "Failed to fill. $0",
          SslErrorCategory::get().message(SSL_get_error(ssl_, rv)));
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
    case SSL_ERROR_SYSCALL:
      RAISE_ERRNO(errno);
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
      close();
      break;
    default:
      logDebug("SSL", "Failed to flush. $0",
          SslErrorCategory::get().message(SSL_get_error(ssl_, rv)));
      TRACE("$0 flush(BufferRef, @$1, $2 bytes) failed. error.", this, source.data(), source.size());
      THROW_SSL_ERROR();
  }
  errno = EAGAIN;
  return 0;
}

size_t SslEndPoint::flush(const FileView& view) {
  Buffer buf;
  FileUtil::read(view, &buf);
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
  RefPtr<TcpEndPoint> _guard(this);
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
      TcpEndPoint::wantFill();
      break;
    case Desire::None:
    case Desire::Write:
      TRACE("$0 wantFlush: write", this);
      TcpEndPoint::wantFlush();
      break;
  }
}

std::string SslEndPoint::toString() const {
  char buf[64];
  int n = snprintf(buf, sizeof(buf), "SslEndPoint(fd=%d)", handle());
  return std::string(buf, n);
}

void SslEndPoint::onClientHandshake(Promise<RefPtr<SslEndPoint>> promise) {
  TRACE("onClientHandshake");
  int rv = SSL_connect(ssl_);
  switch (SSL_get_error(ssl_, rv)) {
    case SSL_ERROR_NONE:
      TRACE("onClientHandshake: succeed");
      onClientHandshakeDone();
      break;
    case SSL_ERROR_WANT_READ:
      TRACE("onClientHandshake: wait for read");
      executor_->executeOnReadable(handle_, std::bind(&SslEndPoint::onClientHandshake, this, promise));
      break;
    case SSL_ERROR_WANT_WRITE:
      TRACE("onClientHandshake: wait for write");
      executor_->executeOnWritable(handle_, std::bind(&SslEndPoint::onClientHandshake, this, promise));
      break;
    case SSL_ERROR_SYSCALL:
    case SSL_ERROR_SSL:
    default: {
      std::error_code ec = makeSslError(ERR_get_error());
      promise.failure(ec);
      logDebug("SSL", "Client handshake error. $0", ec.message());
      TcpEndPoint::close();
      unref(); // XXX
      break;
    }
  }
}

void SslEndPoint::onClientHandshakeDone() {
  if (X509* cert = SSL_get_peer_certificate(ssl_)) {
    // ...
    X509_free(cert);
  }

  // create application connection layer (based on ALPN negotiation)
  connectionFactory_(applicationProtocolName().str(), this);

  if (connection()) {
    TRACE("Initializing application protocol.");
    connection()->onOpen(false);
  } else {
    TRACE("Couldn't create application protocol layer. closing.");
    close();
  }
}

void SslEndPoint::onServerHandshake() {
  TRACE("$0 onServerHandshake begin...", this);
  int rv = SSL_accept(ssl_);
  if (rv <= 0) {
    switch (SSL_get_error(ssl_, rv)) {
      case SSL_ERROR_WANT_READ:
        TRACE("$0 onServerHandshake (want read)", this);
        io_ = executor_->executeOnReadable(
            handle(), std::bind(&SslEndPoint::onServerHandshake, this));
        break;
      case SSL_ERROR_WANT_WRITE:
        TRACE("$0 onServerHandshake (want write)", this);
        io_ = executor_->executeOnWritable(
            handle(), std::bind(&SslEndPoint::onServerHandshake, this));
        break;
      default: {
        char buf[256];
        ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
        logDebug("SSL", "Handshake error. $0", buf);

        TcpEndPoint::close();
        return;
      }
    }
  } else {
    // create associated Connection object and run it
    bioDesire_ = Desire::None;
    RefPtr<TcpEndPoint> _guard(this);
    std::string protocolName = applicationProtocolName().str();
    TRACE("$0 handshake complete (next protocol: \"$1\")", this, protocolName.c_str());

    connectionFactory_(protocolName, this);
    if (connection()) {
      connection()->onOpen(false);
    } else {
      close();
    }
  }
}

void SslEndPoint::flushable() {
  TRACE("$0 flushable()", this);
  RefPtr<TcpEndPoint> _guard(this);
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
      close();
    }
  }
}

BufferRef SslEndPoint::applicationProtocolName() const {
  const unsigned char* data = nullptr;
  unsigned int len = 0;

#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation // ALPN
  SSL_get0_alpn_selected(ssl_, &data, &len);
  if (len > 0) {
    return BufferRef((const char*) data, len);
  }
#endif

  return BufferRef();
}

Buffer SslEndPoint::makeProtocolList(const std::vector<std::string>& protos) {
  Buffer out;

  size_t capacity = 0;
  for (const auto& proto: protos) {
    capacity += proto.size() + 1;
  }
  out.reserve(capacity);

  BinaryWriter writer(BufferUtil::writer(&out));
  for (const auto& proto: protos) {
    assert(proto.size() < 0xFF);
    writer.writeString(proto);
  }

  return out;
}

int SslEndPoint::onVerifyCallback(int ok, X509_STORE_CTX *ctx) {
  if (ok) {
    //
  } else {
    //
  }
  return 1; // TODO, like a bool
}

#if !defined(NDEBUG) // {{{
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
  logDebug("ssl", "TLS $1 extension \"$2\" (id=$3), len=$4",
           client_server ? "server" : "client",
           tlsext_type_to_string(type),
           type,
           len);
}
#endif // }}} !NDEBUG

} // namespace xzero
