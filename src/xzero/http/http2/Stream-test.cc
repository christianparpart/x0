// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/Stream.h>
#include <xzero/testing.h>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::http::http2;

// {{{ test helper
std::shared_ptr<Stream> createNopStream(const std::shared_ptr<Stream>& parent,
                                        bool exclusive = false) {
  const int id = 0;
  return std::make_shared<Stream>(id,
                                  parent.get(),
                                  exclusive,
                                  16,
                                  nullptr,  // connection
                                  nullptr,  // executor
                                  nullptr,  // handler
                                  0,        // maxRequestUriLength
                                  0,        // maxRequestBodyLength,
                                  nullptr,  // dateGenerator
                                  nullptr); // outputCompressor
}

std::shared_ptr<Stream> createNopStreamInclusive(const std::shared_ptr<Stream>& parent) {
  return createNopStream(parent, false);
}

std::shared_ptr<Stream> createNopStreamExclusive(const std::shared_ptr<Stream>& parent) {
  return createNopStream(parent, true);
}
// }}}

TEST(DISABLED_http_http2_Stream, dependencies) {
  // RFC 7540, 5.3.1, Figure 3
  auto A = createNopStream(nullptr);
  ASSERT_EQ(nullptr, A->parentStream());
  ASSERT_EQ(0, A->dependentStreamCount());

  auto B = createNopStream(A);
  ASSERT_EQ(1, A->dependentStreamCount());
  ASSERT_EQ(A.get(), B->parentStream());
  ASSERT_EQ(B.get(), A->firstDependantStream());

  auto C = createNopStream(A);
  ASSERT_EQ(2, A->dependentStreamCount());
  ASSERT_EQ(A.get(), B->parentStream());
  ASSERT_EQ(A.get(), C->parentStream());
  ASSERT_EQ(C.get(), A->firstDependantStream());
  ASSERT_EQ(B.get(), C->nextSiblingStream());
}

TEST(DISABLED_http_http2_Stream, dependenciesExclusive) {
  // RFC 7540, 5.3.1, Figure 4
  auto A = createNopStream(nullptr);
  auto B = createNopStream(A);
  auto C = createNopStream(A);
  auto D = createNopStreamExclusive(A);

  ASSERT_EQ(nullptr, A->parentStream());
  ASSERT_EQ(A.get(), D->parentStream());
  ASSERT_EQ(D.get(), B->parentStream());
  ASSERT_EQ(D.get(), C->parentStream());
}

TEST(DISABLED_http_http2_Stream, repriorization_exclusive) {
  // RFC 7540, 5.3.3, Figure 5
  /*
   *   x                x                x
   *   |               / \               |
   *   A              D   A              D
   *  / \            /   / \             |
   * B   C     ==>  F   B   C   ==>      A
   *    / \                 |           /|\
   *   D   E                E          B C F
   *   |                                 |
   *   F                                 E
   *              (intermediate)    (exclusive)
   */

  auto A = createNopStream(nullptr);
  auto B = createNopStream(A);
  auto C = createNopStream(A);
  auto D = createNopStream(C);
  auto E = createNopStream(C);
  auto F = createNopStream(D);

  constexpr bool exclusive = true;
  D->reparent(A->parentStream(), exclusive);

  ASSERT_EQ(nullptr, D->parentStream());

  ASSERT_EQ(D.get(), A->parentStream());

  ASSERT_EQ(A.get(), B->parentStream());
  ASSERT_EQ(A.get(), C->parentStream());
  ASSERT_EQ(A.get(), F->parentStream());

  ASSERT_EQ(C.get(), E->parentStream());
}

TEST(DISABLED_http_http2_Stream, repriorization_inclusive) {
  // RFC 7540, 5.3.3, Figure 5
  /*
   *   x                x                x
   *   |               / \               |
   *   A              D   A              D
   *  / \            /   / \            / \
   * B   C     ==>  F   B   C   ==>    F   A
   *    / \                 |             / \
   *   D   E                E            B   C
   *   |                                     |
   *   F                                     E
   *              (intermediate)   (non-exclusive) ive)
   */

  auto A = createNopStream(nullptr);
  auto B = createNopStream(A);
  auto C = createNopStream(A);
  auto D = createNopStream(C);
  auto E = createNopStream(C);
  auto F = createNopStream(D);

  constexpr bool inclusive = false;
  D->reparent(A->parentStream(), inclusive);

  ASSERT_EQ(nullptr, D->parentStream());

  ASSERT_EQ(D.get(), F->parentStream());
  ASSERT_EQ(D.get(), A->parentStream());

  ASSERT_EQ(A.get(), B->parentStream());
  ASSERT_EQ(A.get(), C->parentStream());

  ASSERT_EQ(C.get(), E->parentStream());
}
