#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/http/client/HttpHealthMonitor.h>
#include <xzero/net/InetEndPoint.h>
#include <xzero/logging.h>

namespace xzero {
namespace http {
namespace client {

#ifndef NDEBUG
# define DEBUG(msg...) logDebug("http.client.HttpClusterMember", msg)
# define TRACE(msg...) logTrace("http.client.HttpClusterMember", msg)
#else
# define DEBUG(msg...) do {} while (0)
# define TRACE(msg...) do {} while (0)
#endif

HttpClusterMember::HttpClusterMember(
    Executor* executor,
    const std::string& name,
    const InetAddress& inetAddress,
    size_t capacity,
    bool enabled,
    bool terminateProtection,
    std::function<void(HttpClusterMember*)> onEnabledChanged,
    std::function<void(HttpClusterRequest*)> onProcessingFailed,
    std::function<void(HttpClusterMember*)> onRelease,
    const std::string& protocol,
    Duration connectTimeout,
    Duration readTimeout,
    Duration writeTimeout,
    const Uri& healthCheckUri,
    Duration healthCheckInterval,
    unsigned healthCheckSuccessThreshold,
    const std::vector<HttpStatus>& healthCheckSuccessCodes,
    StateChangeNotify onHealthStateChange)
    : executor_(executor),
      name_(name),
      inetAddress_(inetAddress),
      capacity_(capacity),
      enabled_(enabled),
      terminateProtection_(terminateProtection),
      onEnabledChanged_(onEnabledChanged),
      onProcessingFailed_(onProcessingFailed),
      onRelease_(onRelease),
      protocol_(protocol),
      connectTimeout_(connectTimeout),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      healthMonitor_(new HttpHealthMonitor(
          executor,
          inetAddress,
          healthCheckUri,
          healthCheckInterval,
          healthCheckSuccessThreshold,
          healthCheckSuccessCodes,
          connectTimeout,
          readTimeout,
          writeTimeout,
          std::bind(onHealthStateChange, this, std::placeholders::_2))),
      clients_() {
}

HttpClusterMember::~HttpClusterMember() {
}

HttpClusterSchedulerStatus HttpClusterMember::tryProcess(HttpClusterRequest* cr) {
  if (!isEnabled())
    return HttpClusterSchedulerStatus::Unavailable;

  if (!healthMonitor_->isOnline())
    return HttpClusterSchedulerStatus::Unavailable;

  std::lock_guard<std::mutex> lock(lock_);

  if (capacity_ && load_.current() >= capacity_)
    return HttpClusterSchedulerStatus::Overloaded;

  TRACE("Processing request by backend $0 $1", name(), inetAddress_);

  //cr->request->responseHeaders.overwrite("X-Director-Backend", name());

  ++load_;
  cr->backend = this;

  if (!process(cr)) {
    --load_;
    cr->backend = nullptr;
    healthMonitor()->setState(HttpHealthMonitor::State::Offline);
    return HttpClusterSchedulerStatus::Unavailable;
  }

  return HttpClusterSchedulerStatus::Success;
}

void HttpClusterMember::release() {
  onRelease_(this);
}

bool HttpClusterMember::process(HttpClusterRequest* cr) {
#if 0
  Future<HttpClient> f = HttpClient::sendAsync(
      inetAddress_,
      cr->requestInfo,
      BufferRef(), // FIXME: requestBody,
      connectTimeout_,
      readTimeout_,
      writeTimeout_,
      cr->executor);

  f.onFailure(std::bind(&HttpClusterMember::onFailure, this,
                        cr, std::placeholders::_1));
  f.onSuccess(std::bind(&HttpClusterMember::onResponseReceived, this,
                        cr, std::placeholders::_1));
#else
  Future<RefPtr<EndPoint>> f = InetEndPoint::connectAsync(
      inetAddress_, connectTimeout_, readTimeout_, writeTimeout_, cr->executor);
  f.onFailure(std::bind(&HttpClusterMember::onFailure, this,
                        cr, std::placeholders::_1));
  f.onSuccess(std::bind(&HttpClusterMember::onConnected, this,
                        cr, std::placeholders::_1));
#endif

  return true;
}

void HttpClusterMember::onFailure(HttpClusterRequest* cr, Status status) {
  --load_;
  healthMonitor()->setState(HttpHealthMonitor::State::Offline);

  cr->backend = nullptr;

  if (onProcessingFailed_) {
    onProcessingFailed_(cr);
  }
}

void HttpClusterMember::onResponseReceived(HttpClusterRequest* cr,
                                           const HttpClient& client) {
  --load_;

  auto isConnectionHeader = [](const std::string& name) -> bool {
    static const std::vector<std::string> connectionHeaderFields = {
      "Connection",
      // "Content-Length",  // XXX we want the upper layer to know the
                            //     content-length in advance
      "Close",
      "Keep-Alive",
      "TE",
      "Trailer",
      "Transfer-Encoding",
      "Upgrade",
    };

    for (const auto& test: connectionHeaderFields)
      if (iequals(name, test))
        return true;

    return false;
  };

  cr->onMessageBegin(client.responseInfo().version(),
                     client.responseInfo().status(),
                     BufferRef(client.responseInfo().reason()));

  for (const HeaderField& field: client.responseInfo().headers()) {
    if (!isConnectionHeader(field.name())) {
      cr->onMessageHeader(BufferRef(field.name()), BufferRef(field.value()));
    }
  }

  cr->onMessageHeaderEnd();
  cr->onMessageContent(client.responseBody());
  cr->onMessageEnd();
}

void HttpClusterMember::onConnected(HttpClusterRequest* cr,
                                    const RefPtr<EndPoint>& ep) {
  auto client = new HttpClient(cr->executor, ep);

  BufferRef requestBody; // TODO

  client->send(cr->requestInfo, requestBody);
  Future<HttpClient*> f = client->completed();
  f.onFailure(std::bind(&HttpClusterMember::onFailure2, this, cr, client, std::placeholders::_1));
  f.onSuccess(std::bind(&HttpClusterMember::onResponseReceived2, this, cr, std::placeholders::_1));
}

void HttpClusterMember::onFailure2(HttpClusterRequest* cr,
                                   HttpClient* client,
                                   Status status) {
  std::unique_ptr<HttpClient> owned(client);
  onFailure(cr, status);
}

void HttpClusterMember::onResponseReceived2(HttpClusterRequest* cr,
                                            HttpClient* client) {
  std::unique_ptr<HttpClient> owned(client);
  onResponseReceived(cr, *client);
}

} // namespace client
} // namespace http
} // namespace xzero
