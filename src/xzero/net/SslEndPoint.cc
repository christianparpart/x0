// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/SslEndPoint.h>
#include <xzero/net/SslContext.h>
#include <xzero/net/TcpConnection.h>
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
#define TRACE(msg...) logTrace("SslEndPoint" msg)
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

Future<std::shared_ptr<SslEndPoint>> SslEndPoint::connect(
      const InetAddress& target,
      Duration connectTimeout,
      Duration readTimeout,
      Duration writeTimeout,
      Executor* executor,
      const std::string& sni,
      const std::vector<std::string>& applicationProtocolsSupported,
      ProtocolCallback connectionFactory) {

  Promise<std::shared_ptr<SslEndPoint>> promise;

  Future<int> fd = TcpUtil::connect(target, connectTimeout, executor);
  fd.onFailure(promise);

  fd.onSuccess([=, endpoint = std::shared_ptr<SslEndPoint>{}](int& socket) mutable {
    TRACE("connect: fd {} connected", socket);
    endpoint = std::make_shared<SslEndPoint>(FileDescriptor{socket},
                                             target.family(),
                                             readTimeout,
                                             writeTimeout,
                                             executor,
                                             sni,
                                             applicationProtocolsSupported,
                                             connectionFactory);

    // XXX inc ref to keep it alive during async Ops
    // TODO: FIXME^^: endpoint->ref();

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
  TRACE("{} SslEndPoint() ctor, cfd={}", (void*) this, handle());
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
    TRACE("SSL TLS ext host_name: '{}'", sni);
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
  TRACE("{} SslEndPoint() ctor, cfd={}", (void*) this, handle());

  ssl_ = SSL_new(defaultContext->get());
  SSL_set_fd(ssl_, handle());

#if !defined(NDEBUG)
  SSL_set_tlsext_debug_callback(ssl_, &SslEndPoint::tlsext_debug_cb);
  SSL_set_tlsext_debug_arg(ssl_, this);
#endif
}

SslEndPoint::~SslEndPoint() {
  TRACE("{} ~SslEndPoint() dtor", (void*) this);

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
  // FIXME unref(); // XXX: <- incremented in client connect
}

void SslEndPoint::shutdown() {
  int rv = SSL_shutdown(ssl_);

  TRACE("{} close: SSL_shutdown -> {}", (void*) this, rv);
  if (rv == 1) {
    TcpEndPoint::close();
    // FIXME unref(); // XXX: <- incremented in client connect
  } else if (rv == 0) {
    // call again
    shutdown();
  } else {
    switch (SSL_get_error(ssl_, rv)) {
      case SSL_ERROR_SYSCALL:
        // consider done
        TcpEndPoint::close();
        // FIXME unref(); // XXX: <- incremented in client connect
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

size_t SslEndPoint::read(Buffer* sink, size_t space) {
  int rv = SSL_read(ssl_, sink->end(), space);
  if (rv > 0) {
    TRACE("{} read(Buffer:{}) -> {}", (void*) this, space, rv);
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
      TRACE("{} read(Buffer:{}) -> want read", (void*) this, space);
      bioDesire_ = Desire::Read;
      break;
    case SSL_ERROR_WANT_WRITE:
      TRACE("{} read(Buffer:{}) -> want write", (void*) this, space);
      bioDesire_ = Desire::Write;
      break;
    case SSL_ERROR_ZERO_RETURN:
      TRACE("{} read(Buffer:{}) -> remote endpoint closed", (void*) this, space);
      close();
      break;
    default:
      logDebug("SslEndPoint: Failed to fill. {}",
          SslErrorCategory::get().message(SSL_get_error(ssl_, rv)));
      TRACE("{} read(Buffer:{}): SSL_read() -> {}",
            (void*) this, space, SSL_get_error(ssl_, rv));
      THROW_SSL_ERROR();
  }
  errno = EAGAIN;
  return 0;
}

size_t SslEndPoint::write(const BufferRef& source) {
  int rv = SSL_write(ssl_, source.data(), source.size());
  if (rv > 0) {
    bioDesire_ = Desire::None;
    TRACE("{} write(BufferRef, {}, {}/{} bytes)",
          (void*) this, source.data(), rv, source.size());

    return rv;
  }

  switch (SSL_get_error(ssl_, rv)) {
    case SSL_ERROR_SYSCALL:
      RAISE_ERRNO(errno);
    case SSL_ERROR_WANT_READ:
      TRACE("{} write(BufferRef, @{}, {} bytes) failed -> want read.", (void*) this, source.data(), source.size());
      bioDesire_ = Desire::Read;
      break;
    case SSL_ERROR_WANT_WRITE:
      TRACE("{} write(BufferRef, @{}, {} bytes) failed -> want write.", (void*) this, source.data(), source.size());
      bioDesire_ = Desire::Read;
      break;
    case SSL_ERROR_ZERO_RETURN:
      TRACE("{} write(BufferRef, @{}, {} bytes) failed -> remote endpoint closed.", (void*) this, source.data(), source.size());
      close();
      break;
    default:
      logDebug("SslEndPoint: Failed to flush. {}",
          SslErrorCategory::get().message(SSL_get_error(ssl_, rv)));
      TRACE("{} write(BufferRef, @{}, {} bytes) failed. error.", (void*) this, source.data(), source.size());
      THROW_SSL_ERROR();
  }
  errno = EAGAIN;
  return 0;
}

size_t SslEndPoint::write(const FileView& view) {
  Buffer buf;
  FileUtil::read(view, &buf);
  return write(buf);
}

void SslEndPoint::wantRead() {
  if (io_) {
    TRACE("{} wantRead: ignored due to active io", (void*) this);
    return;
  }

  switch (bioDesire_) {
    case Desire::None:
    case Desire::Read:
      TRACE("{} wantRead: read", (void*) this);
      io_ = executor_->executeOnReadable(
          handle(),
          std::bind(&SslEndPoint::fillable, this));
      break;
    case Desire::Write:
      TRACE("{} wantRead: write", (void*) this);
      io_ = executor_->executeOnWritable(
          handle(),
          std::bind(&SslEndPoint::fillable, this));
      break;
  }
}

void SslEndPoint::fillable() {
  TRACE("{} fillable()", (void*) this);
  std::shared_ptr<TcpEndPoint> _guard(shared_from_this());
  try {
    io_.reset();
    bioDesire_ = Desire::None;
    connection()->onReadable();
  } catch (const std::exception& e) {
    connection()->onInterestFailure(e);
  }
}

void SslEndPoint::wantWrite() {
  if (io_) {
    TRACE("{} wantWrite: ignored due to active io", (void*) this);
    return;
  }

  switch (bioDesire_) {
    case Desire::Read:
      TRACE("{} wantWrite: read", (void*) this);
      TcpEndPoint::wantRead();
      break;
    case Desire::None:
    case Desire::Write:
      TRACE("{} wantWrite: write", (void*) this);
      TcpEndPoint::wantWrite();
      break;
  }
}

std::string SslEndPoint::toString() const {
  char buf[64];
  int n = snprintf(buf, sizeof(buf), "SslEndPoint(fd=%d)", handle());
  return std::string(buf, n);
}

void SslEndPoint::onClientHandshake(Promise<std::shared_ptr<SslEndPoint>> promise) {
  int rv = SSL_connect(ssl_);
  switch (SSL_get_error(ssl_, rv)) {
    case SSL_ERROR_NONE:
      onClientHandshakeDone(promise);
      break;
    case SSL_ERROR_WANT_READ:
      executor_->executeOnReadable(handle_, std::bind(&SslEndPoint::onClientHandshake, this, promise));
      break;
    case SSL_ERROR_WANT_WRITE:
      executor_->executeOnWritable(handle_, std::bind(&SslEndPoint::onClientHandshake, this, promise));
      break;
    case SSL_ERROR_SYSCALL:
    case SSL_ERROR_SSL:
    default: {
      std::error_code ec = makeSslError(ERR_get_error());
      promise.failure(ec);
      logDebug("SslEndPoint: Client handshake error. {}", ec.message());
      TcpEndPoint::close();
      // FIXME unref(); // XXX: <- incremented in client connect
      break;
    }
  }
}

void SslEndPoint::onClientHandshakeDone(Promise<std::shared_ptr<SslEndPoint>> promise) {
  if (X509* cert = SSL_get_peer_certificate(ssl_)) {
    // ...
    X509_free(cert);
  }

  // create application connection layer (based on ALPN negotiation)
  connectionFactory_(applicationProtocolName().str(), this);

  if (connection()) {
    connection()->onOpen(false);
    promise.success(std::shared_ptr<SslEndPoint>(this));
  } else {
    TRACE("Couldn't create application protocol layer. closing.");
    close();
  }
}

void SslEndPoint::onServerHandshake() {
  TRACE("{} onServerHandshake begin...", (void*) this);
  int rv = SSL_accept(ssl_);
  if (rv <= 0) {
    switch (SSL_get_error(ssl_, rv)) {
      case SSL_ERROR_WANT_READ:
        TRACE("{} onServerHandshake (want read)", (void*) this);
        io_ = executor_->executeOnReadable(
            handle(), std::bind(&SslEndPoint::onServerHandshake, this));
        break;
      case SSL_ERROR_WANT_WRITE:
        TRACE("{} onServerHandshake (want write)", (void*) this);
        io_ = executor_->executeOnWritable(
            handle(), std::bind(&SslEndPoint::onServerHandshake, this));
        break;
      default: {
        char buf[256];
        ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
        logDebug("SslEndPoint: Handshake error. {}", buf);

        TcpEndPoint::close();
        return;
      }
    }
  } else {
    // create associated TcpConnection object and run it
    bioDesire_ = Desire::None;
    std::shared_ptr<TcpEndPoint> _guard = shared_from_this();
    std::string protocolName = applicationProtocolName().str();
    TRACE("{} handshake complete (next protocol: \"{}\")", (void*) this, protocolName.c_str());

    connectionFactory_(protocolName, this);
    if (connection()) {
      connection()->onOpen(false);
    } else {
      close();
    }
  }
}

void SslEndPoint::flushable() {
  TRACE("{} flushable()", (void*) this);
  std::shared_ptr<TcpEndPoint> _guard = shared_from_this();
  try {
    io_.reset();
    bioDesire_ = Desire::None;
    connection()->onWriteable();
  } catch (const std::exception& e) {
    connection()->onInterestFailure(e);
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
  logDebug("SslEndPoint: TLS {} extension \"{}\" (id={}), len={}",
           client_server ? "server" : "client",
           tlsext_type_to_string(type),
           type,
           len);
}
#endif // }}} !NDEBUG

} // namespace xzero
