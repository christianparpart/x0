// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/io/MemoryFile.h>
#include <xzero/io/MemoryMap.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/FileDescriptor.h>
#include <fcntl.h>

using namespace xzero;

TEST(MemoryMap, rdonly) {
  const std::string path = "/foo.bar.txt";
  const std::string mimetype = "text/plain";
  const BufferRef data = "hello";
  const UnixTime mtime(BufferRef("Thu, 12 Mar 2015 11:29:02 GMT"));

  MemoryFile file(path, mimetype, data, mtime);

  std::unique_ptr<MemoryMap> mm = file.createMemoryMap(false);

  ASSERT_TRUE(mm.get() != nullptr);
  EXPECT_TRUE(mm->isReadable());
  EXPECT_FALSE(mm->isWritable());
  ASSERT_EQ(5, mm->size());

  const char* mmbuf = (const char*) mm->data();
  EXPECT_EQ('h', mmbuf[0]);
  EXPECT_EQ('e', mmbuf[1]);
  EXPECT_EQ('l', mmbuf[2]);
  EXPECT_EQ('l', mmbuf[3]);
  EXPECT_EQ('o', mmbuf[4]);
}

TEST(MemoryMap, readAndWritable) {
  const std::string path = "/foo.bar.txt";
  const std::string mimetype = "text/plain";
  const BufferRef data = "hello";
  const UnixTime mtime(BufferRef("Thu, 12 Mar 2015 11:29:02 GMT"));

  MemoryFile file(path, mimetype, data, mtime);

  { // mutate and verify file contents
    std::unique_ptr<MemoryMap> mm = file.createMemoryMap(true);
    ASSERT_TRUE(mm.get() != nullptr);
    ASSERT_EQ(5, mm->size());
    EXPECT_TRUE(mm->isReadable());
    EXPECT_TRUE(mm->isWritable());

    char* in = (char*) mm->data();
    in[0] = 'a';
    in[1] = 'b';
    in[2] = 'c';
    in[3] = 'd';
    in[4] = 'e';
  }

  { // readout and verify file contents
    FileDescriptor fd = file.createPosixChannel(File::Read);
    Buffer out = FileUtil::read(fd);

    EXPECT_EQ('a', out[0]);
    EXPECT_EQ('b', out[1]);
    EXPECT_EQ('c', out[2]);
    EXPECT_EQ('d', out[3]);
    EXPECT_EQ('e', out[4]);
  }
}
