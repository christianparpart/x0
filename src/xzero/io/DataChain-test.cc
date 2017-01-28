// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/io/DataChain.h>
#include <xzero/io/DataChainListener.h>
#include <xzero/io/FileUtil.h>

using namespace xzero;

TEST(DataChain, cstring) {
  DataChain source;
  source.write("Hello");

  Buffer sink;
  source.transferTo(&sink);
  EXPECT_EQ(5, sink.size());
  EXPECT_EQ("Hello", sink);
}

TEST(DataChain, many_chunks) {
  DataChain source;
  source.write("Hello");
  source.write(" ");
  source.write("World");

  Buffer sink;
  source.transferTo(&sink);
  EXPECT_EQ(11, sink.size());
  EXPECT_EQ("Hello World", sink);
}

TEST(DataChain, transfer_partial_from_buffer) {
  DataChain source;
  source.write("Hello World");

  Buffer sink;
  source.transferTo(&sink, 5);
  EXPECT_EQ(5, sink.size());
  EXPECT_EQ("Hello", sink);
  EXPECT_EQ(6, source.size());

  sink.clear();
  source.transferTo(&sink, 128);
  EXPECT_EQ(6, sink.size());
  EXPECT_EQ(" World", sink);
}

TEST(DataChain, file) {
  FileDescriptor fd = FileUtil::createTempFile();
  FileUtil::write(fd, "Hello World");

  DataChain source;
  source.write(FileView(std::move(fd), 0, 11));

  Buffer sink;
  source.transferTo(&sink);
  EXPECT_EQ(11, sink.size());
  EXPECT_EQ("Hello World", sink);
}


TEST(DataChain, transfer_partial_from_file) {
  FileDescriptor fd = FileUtil::createTempFile();
  FileUtil::write(fd, "Hello World");

  DataChain source;
  source.write(FileView(std::move(fd), 0, 11));

  Buffer sink;
  source.transferTo(&sink, 5);
  EXPECT_EQ(5, sink.size());
  EXPECT_EQ("Hello", sink);
  EXPECT_EQ(6, source.size());

  sink.clear();
  source.transferTo(&sink, 128);
  EXPECT_EQ(6, sink.size());
  EXPECT_EQ(" World", sink);
}

TEST(DataChain, get_n_buffer) {
  DataChain source;
  source.write("Hello World");

  auto chunk = source.get(5);
  ASSERT_TRUE(chunk != nullptr);
  EXPECT_EQ(6, source.size());

  source.write(std::move(chunk));
  EXPECT_EQ(11, source.size());

  Buffer sink;
  source.transferTo(&sink);
  EXPECT_EQ(11, sink.size());
  EXPECT_EQ(" WorldHello", sink);
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

  Buffer sink;
  source.transferTo(&sink);
  ASSERT_EQ(0, source.size());
  EXPECT_EQ(11, sink.size());
  EXPECT_EQ(" WorldHello", sink);
}
