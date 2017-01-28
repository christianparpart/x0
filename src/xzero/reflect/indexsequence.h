// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

namespace xzero {
namespace reflect {

template <int...>
struct IndexSequence {};

template <int N, int... I>
struct IndexSequenceFor :
    IndexSequenceFor<N - 1, N - 1, I...> {};

template <int... I>
struct IndexSequenceFor<0, I...>{
  typedef IndexSequence<I...> IndexSequenceType;
};

template <typename... T>
struct MkIndexSequenceFor {
  typedef typename IndexSequenceFor<sizeof...(T)>::IndexSequenceType type;
};

} // namespace reflect
} // namespace xzero
