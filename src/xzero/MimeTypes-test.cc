// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/MimeTypes.h>
#include <xzero/testing.h>

TEST(MimeTypes, loadFromString) {
  xzero::MimeTypes mimetypes;
  mimetypes.loadFromString("text/plain\ttxt text\n");

  ASSERT_FALSE(mimetypes.empty());

  EXPECT_EQ("text/plain", mimetypes.getMimeType("/fnord.txt"));
  EXPECT_EQ("text/plain", mimetypes.getMimeType("/fnord.text"));

  EXPECT_EQ("application/octet-stream", mimetypes.getMimeType("/fnord.yeah"));
}
