// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/MimeTypes.h>
#include <xzero/testing.h>

TEST(MimeTypes, getMimeType_tilde_backup) {
  xzero::MimeTypes mimetypes;
  EXPECT_EQ("application/x-trash", mimetypes.getMimeType("/file.txt~"));
}

TEST(MimeTypes, getMimeType_found) {
  xzero::MimeTypes mimetypes{"application/octet-stream", {
    {"json", "application/json"},
    {"text", "text/plain"},
    {"txt", "text/plain"},
  }};

  EXPECT_EQ("text/plain", mimetypes.getMimeType("/fnord.txt"));
  EXPECT_EQ("text/plain", mimetypes.getMimeType("/fnord.text"));
}

TEST(MimeTypes, getMimeType_notfound) {
  xzero::MimeTypes mimetypes{"application/octet-stream", {
    {"json", "application/json"},
    {"text", "text/plain"},
    {"txt", "text/plain"},
  }};

  EXPECT_EQ("application/octet-stream", mimetypes.getMimeType("/fnord.yeah"));
}

TEST(MimeTypes, loadFromString) {
  xzero::MimeTypes mimetypes;
  mimetypes.loadFromString("text/plain\ttxt text\n");
  EXPECT_EQ(2, mimetypes.size());
  EXPECT_EQ("text/plain", mimetypes.getMimeType("/hello.txt"));
  EXPECT_EQ("text/plain", mimetypes.getMimeType("/hello.text"));
}
