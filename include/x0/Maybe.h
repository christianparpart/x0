// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <memory>
#include <cstdint>
#include <cassert>

namespace x0 {

/**
 * Maybe<int> maybe = None();
 */
struct None {
    None() {}
};

// helper for Some(T value)
template<typename T>
struct _Some {
    explicit _Some(T v) : value(v) {}
    const T value;
};

/**
 * Maybe<int> maybe = Some(42);
 */
template<typename T>
_Some<T> Some(T v) {
    return _Some<T>(v);
}

/**
 * Scala-alike Maybe type.
 */
template<typename T>
class Maybe {
public:
    Maybe() : value_(nullptr) {}

    Maybe(const T& value) : value_(reinterpret_cast<T*>(storage_)) {
        new (value_) T (value);
    }

    ~Maybe() {
        clear();
    }

    void clear() {
        if (isSome()) {
            value_->~T();
            value_ = nullptr;
        }
    }

    Maybe(const _Some<T>& some) {
        value_ = reinterpret_cast<T*>(storage_);
        new (value_) T (some.value);
    }

    Maybe(const None& n) : value_(nullptr) {
    }

    Maybe<T>& operator=(const Maybe<T>& other) {
        clear();

        if (other.isSome()) {
            value_ = reinterpret_cast<T*>(storage_);
            new (value_) T (*other.value_);
        }

        return *this;
    }

    Maybe(Maybe<T>&& other) : value_(nullptr) {
        if (other.isSome()) {
            value_ = reinterpret_cast<T*>(storage_);
            *value_ = std::move(*other.value_);
            other.clear();
        }
    }

    Maybe<T>& operator=(Maybe<T>&& other) {
        if (isSome() && other.isSome()) {
            *value_ = std::move(*other.value_);
            other.clear();
        } else if (other.isSome()) {
            value_ = reinterpret_cast<T*>(storage_);
            new (value_) T (std::move(*other.value_));
            other.clear();
        }
        return *this;
    }

    bool isNone() const { return value_ == nullptr; }
    bool isSome() const { return value_ != nullptr; }

    T& get() { expectSome(); return *value_; }
    const T& get() const { expectSome(); return *value_; }
    T getOrElse(T alt) const { return isSome() ? *value_ : alt; }

    T& operator*() { return get(); }
    const T& operator*() const { return get(); }

    T* operator->() { expectSome(); return value_; }
    const T* operator->() const { expectSome(); return value_; }

    // collection support
    bool empty() const { return isNone(); }
    size_t size() const { return isSome() ? 1 : 0; }

    T& operator[](size_t i) { assert(i == 0); return *get(); }
    const T& operator[](size_t i) const { assert(i == 0); return *get(); }

    class iterator;
    iterator begin() { return iterator(isSome() ? this : nullptr); }
    iterator end() { return iterator(nullptr); }

    void expectSome() const {
        assert(isSome() && "value required.");
    }

    class Block;

    template<typename U>
    Block maybe(U trueBlock) const {
        if (isSome()) {
            trueBlock(get());
            return Block(true);
        } else {
            return Block(false);
        }
    }

private:
    T* value_;
    uint8_t storage_[sizeof(T)];
};

template<typename T> class Maybe<T>::iterator { // {{{
    Maybe<T>* self_;

public:
    explicit iterator(Maybe<T>* self) : self_(self) {}
    iterator() : self_(nullptr) {}

    bool operator==(const iterator& other) const {
        if (self_ == other.self_)
            return true;

        return false;
    }

    bool operator!=(const iterator& other) const {
        return !(*this == other);
    }

    T& operator*() { return self_->get(); }
    const T& operator*() const { return self_->get(); }

    T* operator->() { return self_->get(); }
    const T* operator->() const { return self_->get(); }

    iterator& operator++() {
        self_ = nullptr;
        return *this;
    }
}; // }}}

template<typename T> inline bool operator==(const Maybe<T>& a, const Maybe<T>& b) {
    return (a.isNone() && b.isNone())
        || (a.isSome() && b.isSome() && a.get() == b.get());
}

template<typename T> inline bool operator!=(const Maybe<T>& a, const Maybe<T>& b) {
    return !(a == b);
}

template<typename T> class Maybe<T>::Block { // {{{
public:
    explicit Block(bool result) : result_(result) {}

    Block otherwise(std::function<void()> falseBlock) {
        if (!result_) {
            falseBlock();
        }
        return *this;
    }

    bool get() const { return result_; }

private:
    bool result_;
}; // }}}

/**
 * Invokes given lambda if value contains something.
 */
template<typename T, typename U>
inline typename Maybe<T>::Block maybe_if(const Maybe<T>& value, U trueBlock) {
    return value.maybe(trueBlock);
}

} // namespace x0
