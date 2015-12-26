// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <gtest/gtest.h>
#include <xzero/http/http2/Generator.h>
#include <xzero/DataChain.h>

using namespace xzero;
using namespace xzero::http::http2;

class BufferSink : public DataChainSink { // {{{
 public:
  size_t transfer(const BufferRef& chunk) override {
    buffer_.push_back(chunk);
    return chunk.size();
  }

  size_t transfer(const FileView& chunk) override {
    chunk.fill(&buffer_);
    return chunk.size();
  }

  const Buffer& get() const noexcept { return buffer_; }
  const Buffer* operator->() const noexcept { return &buffer_; }
  const Buffer& operator*() const noexcept { return buffer_; }
  void clear() { buffer_.clear(); }

 private:
  Buffer buffer_;
}; // }}}

TEST(http_http2_Generator, settings) {
  DataChain chain;
  Generator generator(&chain);

  generator.generateSettings({
      {SettingParameter::EnablePush, 1},
      {SettingParameter::MaxConcurrentStreams, 16},
      {SettingParameter::InitialWindowSize, 42},
  });

  BufferSink sink;
  chain.transferTo(&sink);

  ASSERT_EQ(9 + 3 * 6, sink->size());
  // TODO: some binary comparison of what we expect
}

TEST(http_http2_Generator, settingsAck) {
  DataChain chain;
  Generator generator(&chain);

  generator.generateSettingsAcknowledgement();

  BufferSink sink;
  chain.transferTo(&sink);

  ASSERT_EQ(9, sink->size());
  // TODO: some binary comparison of what we expect
}
