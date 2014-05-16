/* <x0/Option.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <memory>
#include <cassert>

namespace x0 {

/**
 * Option<int> maybe = None();
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
 * Option<int> maybe = Some(42);
 */
template<typename T>
_Some<T> Some(T v) {
    return _Some<T>(v);
}

/**
 * Scala-alike Option type.
 */
template<typename T>
class Option {
public:
    explicit Option(const T& value) : state_(SOME), value_(value) {}
    Option(const None& n) : state_(NONE), value_() {}
    Option(const _Some<T>& some) : state_(SOME), value_(some.value) {}

    Option<T>& operator=(const Option<T>& other) {
        state_ = other.state_;
        value_ = other.value_;
        return *this;
    }

    Option(Option<T>&& other) : state_(other.state_), value_(std::move(other.value_)) { other.state_ = NONE; }
    Option<T>& operator=(Option<T>&& other) {
        state_ = other.state_;
        value_ = std::move(other.value_);
        other.state_ = NONE;
        return *this;
    }

    bool isNone() const { return state_ == NONE; }
    bool isSome() const { return state_ == SOME; }

    T& get() { assert(isSome()); return value_; }
    const T& get() const { assert(isSome()); return value_; }
    const T& getOrElse(const T& alt) const { return isSome() ? value_ : alt; }

    // collection support
    bool empty() const { return isNone(); }
    size_t size() const { return isSome() ? 1 : 0; }

    T& operator[](size_t i) { assert(i == 0); return get(); }
    const T& operator[](size_t i) const { assert(i == 0); return get(); }

    class iterator;
    iterator begin() { return iterator(this); }
    iterator end() { return iterator(); }

private:
    enum State { SOME, NONE } state_ ;
    T value_;
};

template<typename T> class Option<T>::iterator { // {{{
    State state_;
    Option<T>* self_;

public:
    iterator(Option<T>* self) : state_(self->state_), self_(self) {}
    iterator() : state_(NONE), self_(nullptr) {}

    bool operator==(const iterator& other) const {
        if (state_ != other.state_)
            return false;

        return true; //state_ == SOME && *self_ == *other.self_;
    }

    bool operator!=(const iterator& other) const {
        return !(*this == other);
    }

    T& operator*() { return self_->get(); }
    const T& operator*() const { return self_->get(); }

    T& operator->() { return self_->get(); }
    const T& operator->() const { return self_->get(); }

    iterator& operator++() {
        state_ = NONE;
        self_ = nullptr;
        return *this;
    }
}; // }}}

template<typename T> bool operator==(const Option<T>& a, const Option<T>& b) {
    return (a.isNone() && b.isNone())
        || (a.isSome() && b.isSome() && a.get() == b.get());
}

template<typename T> bool operator!=(const Option<T>& a, const Option<T>& b) {
    return !(a == b);
}

/* === Example:

    Option<int> si = Some(42);
    assert(si.isSome());

    printf("si.get = %d\n", si.get());

    Option<int> ni = None();
    assert(ni.isNone());

    printf("ni.getOrElse = %d\n", ni.getOrElse(-1));

*/

} // namespace x0
