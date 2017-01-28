// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/HttpTransport.h>
#include <xzero/http/HttpChannel.h>
#include <xzero/http/http2/Flow.h>
#include <xzero/http/http2/StreamID.h>
#include <xzero/io/DataChain.h>

namespace xzero {

class Executor;

namespace http {

class HttpDateGenerator;
class HttpOutputCompressor;

namespace http2 {

class Connection;
class Stream;

bool streamCompare(Stream* a, Stream* b);

class Stream : public ::xzero::http::HttpTransport {
 public:
  Stream(StreamID id,
         Stream* parentStream,
         bool exclusiveDependency,
         unsigned weight,
         Connection* connection,
         Executor* executor,
         const HttpHandler& handler,
         size_t maxRequestUriLength,
         size_t maxRequestBodyLength,
         HttpDateGenerator* dateGenerator,
         HttpOutputCompressor* outputCompressor);

  /**
   * HTTP/2 stream ID.
   */
  unsigned id() const noexcept;

  /**
   * HTTP semantic layer.
   */
  HttpChannel* channel() const noexcept;

  /**
   * Stream Priority Management
   */
  ///@{

  /** Proportional stream weight, a value between 1 and 256.  */
  unsigned weight() const noexcept;

  /** current parent stream, a stream this stream depends on. */
  Stream* parentStream() const noexcept;

  /**
   * Next stream in line that also shares the same parent stream,
   * or NULL if none. */
  Stream* nextSiblingStream() const noexcept;

  /**
   * First stream in line that shares this stream as parent stream,
   * or NULL if none. */
  Stream* firstDependantStream() const noexcept;

  /** retrieves the number of total direct dependent streams. */
  size_t dependentStreamCount() const noexcept;

  /** Tests if given @p other stream is an ancestor (indirect parent) of this stream. */
  bool isAncestor(const Stream* other) const noexcept;

  /** Tests if given @p other stream is a descendant (indirect dependent) of this stream. */
  bool isDescendant(const Stream* other) const noexcept;

  class sibling_iterator { // {{{
   public:
    sibling_iterator(Stream* first)
        : current_(first) {}

    sibling_iterator& operator++() {
      if (current_)
        current_ = current_->nextSiblingStream_;

      return *this;
    }

    sibling_iterator& operator++(int) {
      if (current_)
        current_ = current_->nextSiblingStream_;

      return *this;
    }

    Stream* operator->() const {
      return current_;
    }

    Stream* operator*() const {
      return current_;
    }

    bool operator==(const sibling_iterator& other) const noexcept {
      return current_ == other.current_;
    }

    bool operator!=(const sibling_iterator& other) const noexcept {
      return current_ != other.current_;
    }

   private:
    Stream* current_;
  };

  class SiblingIteratorWrapper {
   public:
    SiblingIteratorWrapper(Stream* s)
        : sibling_(s) {}

    sibling_iterator begin() const { return sibling_iterator(sibling_); }
    sibling_iterator end() const { return sibling_iterator(nullptr); }

    sibling_iterator cbegin() const { return sibling_iterator(sibling_); }
    sibling_iterator cend() const { return sibling_iterator(nullptr); }
   private:
    Stream* sibling_;
  };
  // }}}

  /**
   * Retrieves an iterator-friendly wrapper object of direct dependent streams.
   */
  SiblingIteratorWrapper dependentStreams() const;

  /**
   * Reparents this stream to be dependent on new @p newParent.
   *
   * @param newParent the new parent stream.
   * @param exclusive indicates reparenting to be exclusive or inclusive.
   */
  void reparent(Stream* newParent, bool exclusive);

  ///@}

  void sendWindowUpdate(size_t windowSize);
  void appendBody(const BufferRef& data);
  void handleRequest();

 public:
  void setCompleter(CompletionHandler onComplete);
  void invokeCompleter(bool success);

  void sendHeaders(const HttpResponseInfo& info);
  void closeInput();
  void closeOutput();

  // HttpTransport overrides
  void abort() override;
  void completed() override;
  void send(HttpResponseInfo& responseInfo, Buffer&& chunk,
            CompletionHandler onComplete) override;
  void send(HttpResponseInfo& responseInfo, const BufferRef& chunk,
            CompletionHandler onComplete) override;
  void send(HttpResponseInfo& responseInfo, FileView&& chunk,
            CompletionHandler onComplete) override;
  void send(Buffer&& chunk, CompletionHandler onComplete) override;
  void send(const BufferRef& chunk, CompletionHandler onComplete) override;
  void send(FileView&& chunk, CompletionHandler onComplete) override;

 protected:
  /**
   * Links @p node right into the doubly-linked list right after
   * predecessor @p pred.
   *
   * @param node stream to link into the list.
   * @param pred predecessor node the will preceed node in the linked list.
   */
  void linkSibling(Stream* node, Stream* pred);

  /**
   * Unlinked @p node from its doubly linked list.
   */
  void unlinkSibling(Stream* node);

 private:
  Connection* connection_;                // HTTP/2 connection layer
  std::unique_ptr<HttpChannel> channel_;  // HTTP semantics layer
  StreamID id_;                           // stream id
  bool inputClosed_;                      // remote endpoint has closed
  bool outputClosed_;                     // local endpoint has closed

  Stream* parentStream_;                  // parent stream this one depends on
  Stream* prevSiblingStream_;             // prev sibling with the same parent
  Stream* nextSiblingStream_;             // next sibling with the same parent
  Stream* firstDependantStream_;          // first dependent stream
  unsigned weight_;                       // stream dependency bandwidth weight

  Flow inputFlow_;                        // flow for receiving stream frames
  Flow outputFlow_;                       // flow for transmitted stream frames
  DataChain body_;                        // pending response body chunks
  CompletionHandler onComplete_;
};

// {{{ inlines
inline unsigned Stream::id() const noexcept {
  return id_;
}

inline HttpChannel* Stream::channel() const noexcept {
  return channel_.get();
}

inline Stream* Stream::parentStream() const noexcept {
  return parentStream_;
}

inline Stream* Stream::nextSiblingStream() const noexcept {
  return nextSiblingStream_;
}

inline Stream* Stream::firstDependantStream() const noexcept {
  return firstDependantStream_;
}

inline size_t Stream::dependentStreamCount() const noexcept {
  size_t n = 0;
  Stream* s = firstDependantStream_;

  while (s != nullptr) {
    s = s->nextSiblingStream_;
    n++;
  }

  return n;
}

inline unsigned Stream::weight() const noexcept {
  return weight_;
}

inline Stream::SiblingIteratorWrapper Stream::dependentStreams() const {
  return SiblingIteratorWrapper(firstDependantStream_);
}

inline bool streamCompare(Stream* a, Stream* b) {
  return a->id() == b->id();
}
// }}}

} // namespace http2
} // namespace http
} // namespace xzero
