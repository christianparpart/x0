// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/fastcgi/Generator.h>
#include <xzero/net/EndPointWriter.h>
#include <xzero/net/ByteArrayEndPoint.h>
#include <xzero/logging.h>
#include <xzero/testing.h>

using namespace xzero;
using namespace xzero::http;

TEST(http_fastcgi_Generator, simpleRequest) {
  constexpr BufferRef content = "hello, world";
  HeaderFieldList headers = {
    {"Foo", "the-foo"},
    {"Bar", "the-bar"}
  };
  HttpRequestInfo info(HttpVersion::VERSION_1_1, "PUT", "/index.html",
                       content.size(), headers);

  EndPointWriter writer;
  http::fastcgi::Generator generator(1, &writer);
  generator.generateRequest(info);
  generator.generateBody(content);
  generator.generateEnd();

  ByteArrayEndPoint ep;
  writer.flush(&ep);

  //printf("%s\n", ep.output().hexdump(HexDumpMode::PrettyAscii).c_str());

  //...
}

TEST(http_fastcgi_Generator, simpleResponse) {
  BufferRef content = "hello, world";
  HeaderFieldList headers = {
    {"Foo", "the-foo"},
    {"Bar", "the-bar"}
  };
  HttpResponseInfo info(HttpVersion::VERSION_1_1, HttpStatus::Ok, "my",
      false, content.size(), headers, {});

  EndPointWriter writer;
  http::fastcgi::Generator generator(1, &writer);
  generator.generateResponse(info);
  generator.generateBody(content);
  generator.generateEnd();

  ByteArrayEndPoint ep;
  writer.flush(&ep);

  //printf("%s\n", ep.output().hexdump(HexDumpMode::PrettyAscii).c_str());

  // TODO: verify
}
