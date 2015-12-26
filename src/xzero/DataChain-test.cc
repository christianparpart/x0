// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <gtest/gtest.h>
#include <xzero/DataChain.h>
#include <xzero/io/FileUtil.h>

using namespace xzero;

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

TEST(DataChain, cstring) {
  DataChain source;
  source.write("Hello");

  BufferSink sink;
  source.transferTo(&sink);
  EXPECT_EQ(5, sink->size());
  EXPECT_EQ("Hello", *sink);
}

TEST(DataChain, many_chunks) {
  DataChain source;
  source.write("Hello");
  source.write(" ");
  source.write("World");

  BufferSink sink;
  source.transferTo(&sink);
  EXPECT_EQ(11, sink->size());
  EXPECT_EQ("Hello World", *sink);
}

TEST(DataChain, transfer_partial_from_buffer) {
  DataChain source;
  source.write("Hello World");

  BufferSink sink;
  source.transferTo(&sink, 5);
  EXPECT_EQ(5, sink->size());
  EXPECT_EQ("Hello", *sink);
  EXPECT_EQ(6, source.size());

  sink.clear();
  source.transferTo(&sink, 128);
  EXPECT_EQ(6, sink->size());
  EXPECT_EQ(" World", *sink);
}

TEST(DataChain, file) {
  FileDescriptor fd = FileUtil::createTempFile();
  FileUtil::write(fd, "Hello World");

  DataChain source;
  source.write(FileView(std::move(fd), 0, 11));

  BufferSink sink;
  source.transferTo(&sink);
  EXPECT_EQ(11, sink->size());
  EXPECT_EQ("Hello World", *sink);
}


TEST(DataChain, transfer_partial_from_file) {
  FileDescriptor fd = FileUtil::createTempFile();
  FileUtil::write(fd, "Hello World");

  DataChain source;
  source.write(FileView(std::move(fd), 0, 11));

  BufferSink sink;
  source.transferTo(&sink, 5);
  EXPECT_EQ(5, sink->size());
  EXPECT_EQ("Hello", *sink);
  EXPECT_EQ(6, source.size());

  sink.clear();
  source.transferTo(&sink, 128);
  EXPECT_EQ(6, sink->size());
  EXPECT_EQ(" World", *sink);
}

TEST(DataChain, get_n_buffer) {
  DataChain source;
  source.write("Hello World");

  auto chunk = source.get(5);
  ASSERT_TRUE(chunk != nullptr);
  EXPECT_EQ(6, source.size());

  source.write(std::move(chunk));
  EXPECT_EQ(11, source.size());

  BufferSink sink;
  source.transferTo(&sink);
  EXPECT_EQ(11, sink->size());
  EXPECT_EQ(" WorldHello", *sink);
}

TEST(DataChain, get_n_file) {
  FileDescriptor fd = FileUtil::createTempFile();
  FileUtil::write(fd, "Hello World");

  DataChain source;
  source.write(FileView(fd, 0, 11, false));

  auto chunk = source.get(5);
  ASSERT_TRUE(chunk != nullptr);
  ASSERT_EQ(6, source.size());

  source.write(std::move(chunk));
  ASSERT_EQ(11, source.size());

  BufferSink sink;
  source.transferTo(&sink);
  ASSERT_EQ(0, source.size());
  EXPECT_EQ(11, sink->size());
  EXPECT_EQ(" WorldHello", *sink);
}
