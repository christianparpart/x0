// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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
