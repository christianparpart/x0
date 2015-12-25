#pragma once

#include <xzero/http/http2/StreamState.h>
#include <xzero/http/HttpChannel.h>

namespace xzero {
namespace http {
namespace http2 {

class Connection;
typedef unsigned StreamID;

bool streamCompare(Stream* a, Stream* b);

typedef DependencyTree<Stream*, streamCompare> StreamTree;
typedef StreamTree::Node StreamTreeNode;

class Stream {
 public:
  Stream(Connection* connection, StreamID id);

  unsigned id() const noexcept;
  StreamState state() const noexcept;
  HttpChannel* channel() const noexcept;

  void sendWindowUpdate(size_t windowSize);

 private:
  Connection* connection;                 // HTTP/2 connection layer
  std::unique_ptr<HttpChannel> channel_;  // HTTP semantics layer
  StreamID id_;                           // stream id
  StreamState state_;                     // default: Idle
  int weight_;                            // default: 16
  StreamTreeNode* node_;                  // ref in the stream dependency tree
};

inline bool streamCompare(Stream* a, Stream* b) {
  return a->id() == b->id();
}

} // namespace http2
} // namespace http
} // namespace xzero
