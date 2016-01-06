// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef _XZERO_STDTYPES_H
#define _XZERO_STDTYPES_H

#include <condition_variable>
#include <ctime>
#include <deque>
#include <inttypes.h>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

namespace xzero {

using String = std::string;
using WString = std::wstring;
using UTF32String = std::string;

template <typename T>
using SharedPtr = std::shared_ptr<T>;

template <typename T>
using UniquePtr = std::unique_ptr<T>;

template <typename T>
using ScopedPtr = std::unique_ptr<T>;

template <typename T>
using ScopedLock = std::unique_lock<T>;

template <typename T>
using Vector = std::vector<T>;

template <typename T>
using Set = std::set<T>;

template <typename T>
using List = std::list<T>;

template <typename T>
using Deque = std::deque<T>;

template <typename T>
using Function = std::function<T>;

template <typename T1, typename T2>
using Pair = std::pair<T1, T2>;

template <typename... T>
using Tuple = std::tuple<T...>;

template <typename T1, typename T2>
using HashMap = std::unordered_map<T1, T2>;

template <typename T1, typename T2>
using OrderedMap = std::map<T1, T2>;

template <typename T>
using Stack = std::stack<T>;

using StandardException = std::exception;

} // namespace xzero
#endif
