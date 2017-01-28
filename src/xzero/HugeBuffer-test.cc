// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/HugeBuffer.h>
#include <xzero/Buffer.h>
#include <xzero/io/InputStream.h>
#include <xzero/io/FileView.h>
#include <xzero/io/FileUtil.h>

using namespace xzero;

TEST(HugeBuffer, ctor_0) {
  HugeBuffer hb(0);

  EXPECT_EQ(0, hb.size());
  EXPECT_TRUE(hb.empty());
  EXPECT_FALSE(hb.isFile());
}

TEST(HugeBuffer, fillToMemory) {
  HugeBuffer hb(5);

  hb.write(BufferRef("Hello"));

  EXPECT_FALSE(hb.empty());
  EXPECT_EQ(5, hb.size());
  EXPECT_FALSE(hb.isFile());
}

TEST(HugeBuffer, fillToFile) {
  HugeBuffer hb(0);

  hb.write(BufferRef("Hello"));

  EXPECT_FALSE(hb.empty());
  EXPECT_EQ(5, hb.size());
  EXPECT_TRUE(hb.isFile());
}

TEST(HugeBuffer, fillToMemThenFile) {
  HugeBuffer hb(5);

  hb.write(BufferRef("Hello"));
  EXPECT_EQ(5, hb.size());
  EXPECT_FALSE(hb.isFile());

  hb.write(BufferRef("World"));
  EXPECT_EQ(10, hb.size());
  EXPECT_TRUE(hb.isFile());
}

TEST(HugeBuffer, reset_memory) {
  HugeBuffer hb(5);

  hb.write(BufferRef("Hello"));
  hb.reset();

  EXPECT_TRUE(hb.empty());
  EXPECT_EQ(0, hb.size());
  EXPECT_EQ("", hb.getBuffer());
}

TEST(HugeBuffer, reset_file) {
  HugeBuffer hb(0);

  hb.write(BufferRef("Hello"));
  EXPECT_EQ("Hello", hb.getBuffer());
  hb.reset();

  EXPECT_TRUE(hb.empty());
  EXPECT_EQ(0, hb.size());
  EXPECT_EQ("", hb.getBuffer());
}

TEST(HugeBuffer, getFileView_memory) {
  HugeBuffer hb(5);
  hb.write(BufferRef("Hello"));

  FileView view = hb.getFileView();
  EXPECT_EQ(5, view.size());

  Buffer out = FileUtil::read(view);
  EXPECT_EQ("Hello", out);
}

TEST(HugeBuffer, getFileView_file) {
  HugeBuffer hb(0);
  hb.write(BufferRef("Hello"));

  FileView view = hb.getFileView();
  EXPECT_EQ(5, view.size());

  Buffer out = FileUtil::read(view);
  EXPECT_EQ("Hello", out);
}

TEST(HugeBuffer, getBuffer_memory) {
  HugeBuffer hb(5);
  hb.write(BufferRef("Hello"));
  EXPECT_EQ("Hello", hb.getBuffer());
}

TEST(HugeBuffer, getBuffer_file) {
  HugeBuffer hb(0);
  hb.write(BufferRef("Hello"));
  EXPECT_EQ("Hello", hb.getBuffer());
}

TEST(HugeBuffer, getInputStream_memory) {
  HugeBuffer hb(5);
  hb.write(BufferRef("Hello"));
  auto stream = hb.getInputStream();

  Buffer out;
  ssize_t readBytes = stream->read(&out, 10);
  EXPECT_EQ("Hello", out);
}

TEST(HugeBuffer, getInputStream_file) {
  HugeBuffer hb(0);
  hb.write(BufferRef("Hello"));
  auto stream = hb.getInputStream();

  Buffer out;
  ssize_t readBytes = stream->read(&out, 10);
  EXPECT_EQ("Hello", out);
}

TEST(HugeBuffer, write_Buffer) {
  HugeBuffer hb(5);
  Buffer input("Hello");
  hb.write(std::move(input));
  EXPECT_EQ(5, hb.size());
  EXPECT_EQ("Hello", hb.getBuffer());
}

TEST(HugeBuffer, write_FileView_memory) {
  FileDescriptor fd = FileUtil::createTempFile();
  FileUtil::write(fd, BufferRef("Hello"));

  FileView view(std::move(fd), 0, 5);

  HugeBuffer hb(5);
  hb.write(view);

  EXPECT_EQ(5, hb.size());
  EXPECT_EQ("Hello", hb.getBuffer());
}

TEST(HugeBuffer, write_FileView_file) {
  FileDescriptor fd = FileUtil::createTempFile();
  FileUtil::write(fd, BufferRef("Hello"));

  FileView view(std::move(fd), 0, 5);

  HugeBuffer hb(0);
  hb.write(view);

  EXPECT_EQ(5, hb.size());
  EXPECT_EQ("Hello", hb.getBuffer());
}
