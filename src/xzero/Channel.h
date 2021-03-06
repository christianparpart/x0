// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <condition_variable>
#include <atomic>
#include <mutex>
#include <deque>
#include <utility>

namespace xzero {

/**
 * Go-like thread channel communication API.
 */
template<typename T, const size_t BufSize = 0>
class Channel {
 public:
  Channel();
  ~Channel();

  void close();
  bool send(T&& value);
  bool send(const T& value);
  bool receive(T* value);

  size_t size() const;
  bool empty() const;
  bool isClosed() const;

  Channel& operator<<(T&& value);
  Channel& operator>>(std::pair<T, bool>* result);
  Channel& operator>>(T* result);

  operator bool() const noexcept { return !isClosed_.load(); }
  bool operator!() const noexcept { return isClosed_.load(); }

 private:
  mutable std::mutex lock_;
  std::deque<T> queue_;
  std::atomic<bool> isClosed_;
  std::condition_variable receiversCond_;
  std::condition_variable sendersCond_;
};

#if 1 == 0 // {{{ WIP: brainstorming the idea of multi-selecting
template<typename... Channel>
int select(Channel& ...channels);

class SelectBuilder {
 public:
  SelectBuilder();
  ~SelectBuilder();

  class CaseStmtBase {
   public:
    virtual void consume() = 0;
  };

  template<typename ChannelT, typename T>
  class CaseStmt {
   public:
    CaseStmt(ChannelT& channel, std::function<void(T&&, bool)> consumer)
        : channel_(channel), consumer(consumer) {}

    void consume() override {
      T value;
      bool ok = channel_.receive(&value);
      consumer_(std::move(value), ok);
    }

   private:
    ChannelT& channel_;
    std::function<void(T&&, bool)> consumer_;
  };

  template<typename ChannelT, typename T>
  SelectBuilder& on(ChannelT& c, std::function<void(T&&, bool)> f);

  SelectBuilder& otherwise(std::function<void()> f);

 private:
  std::list<CaseStmt> selectors_;
};

SelectBuilder channelSelector();
#endif // }}}

} // namespace xzero

#include <xzero/Channel-impl.h>
