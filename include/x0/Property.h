// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef x0_property_h
#define x0_property_h

// C++ template based properties, as defined in: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2004/n1615.pdf

#include <map>
#include <functional>

namespace x0 {

//! \addtogroup base
//@{

template<class T>
class ValueProperty
{
private:
    T value_;

public:
    typedef T value_type;

    ValueProperty() : value_() {}
    ValueProperty(T const& v) : value_(v) {}
    ValueProperty(T&& v) : value_(v) {}

    T operator()() const
    {
        return value_;
    }

    T operator()(T const& v)
    {
        value_ = v;
        return value_;
    }

    T get() const
    {
        return value_;
    }

    T set(T const& v)
    {
        value_ = v;
        return value_;
    }

    operator T() const
    {
        return value_;
    }

    T operator=(T const& v)
    {
        value_ = v;
        return value_;
    }

    T operator=(T&& v)
    {
        value_ = v;
        return value_;
    }

    T operator+=(const T& v)
    {
        value_ += v;
        return value_;
    }
};

/** read-only value property. */
template<class T>
class RValueReadProperty
{
private:
    const T& value_;

public:
    template<class U>
    explicit RValueReadProperty(U v) : value_(v) {}

    T const& operator()() const
    {
        return value_;
    }

    T const& get() const
    {
        return value_;
    }

    operator T const&() const
    {
        return value_;
    }

    typedef T const& value_type;
};

template<
    typename T,
    typename Object,
    T (Object::*real_get)() const
>
class ReadProperty
{
private:
    Object *object_;

public:
    explicit ReadProperty(Object *obj) : object_(obj)
    {
    }

    void operator()(Object *obj)
    {
        object_ = obj;
    }

    T operator()() const
    {
        return (object_->*real_get)();
    }

    T get() const
    {
        return (object_->*real_get)();
    }

    operator T() const
    {
        return (object_->*real_get)();
    }

    typedef T value_type;
};

template<
    typename T,
    typename Object,
    void (Object::*real_set)(T const&)
>
class WriteProperty
{
private:
    Object *object_;
    T value_;

public:
    typedef WriteProperty<T, Object, real_set>  self_type;
    typedef T value_type;

    WriteProperty(Object *obj) :
        object_(obj), value_()
    {
    }

    WriteProperty(Object *obj, T const & v) : object_(obj)
    {
        set(v);
    }

    const T& operator()() const
    {
        return value_;
    }

    void operator()(T const& value)
    {
        (object_->*real_set)(value);
        value_ = value;
    }

    const T& get() const
    {
        return value_;
    }

    void set(T const& value)
    {
        (object_->*real_set)(value);
    }

    self_type& operator=(T const& value)
    {
        (object_->*real_set)(value);
        return *this;
    }
};

template<typename T>
class Property
{
private:
    std::function<T()> get_;
    std::function<T(const T&)> set_;

public:
    template<class Getter, class Setter>
    Property(Getter _get, Setter _set) :
        get_(_get), set_(_set)
    {
    }

    template<class Getter, class Setter>
    Property(Getter _get, Setter _set, const T& v) :
        get_(_get), set_(_set)
    {
        set_(v);
    }

    template<class Getter, class Setter>
    void bind(Getter _get, Setter _set)
    {
        get_ = _get;
        set_ = _set;
    }

    T operator()() const
    {
        return get_();
    }

    T operator()(const T& value)
    {
        return set_(value);
    }

    T get() const
    {
        return get_();
    }

    T set(const T& value)
    {
        return set_(value);
    }

    operator T() const
    {
        return get_();
    }

    T operator=(const T& value)
    {
        return set_(value);
    }

    typedef T value_type;
};

template<
    typename Key,
    typename T,
    class Compare = std::less<Key>,
    class Allocator = std::allocator<std::pair<const Key, T> >
>
class IndexedProperty
{
private:
    std::map<Key, T> data_;

    typedef typename std::map<Key, T, Compare, Allocator>::iterator map_iterator;

public:
    /** retrieve value of given key. */
    T operator()(Key const& key)
    {
        std::pair<map_iterator, bool> result;
        result = data_.insert(std::make_pair(key, T()));
        return (*result.first).second;
    }

    /** sets value for given key. */
    T operator()(Key const& key, T const& v)
    {
        std::pair<map_iterator, bool> result;
        result = data_.insert(std::make_pair(key, v));
        return (*result.first).second;
    }

    /** retrieve value of given key. */
    T get(Key const& key)
    {
        std::pair<map_iterator, bool> result;
        result = data_.insert(std::make_pair(key, T()));
        return (*result.first).second;
    }

    /** sets value for given key. */
    T set(Key const& key, T const& v)
    {
        std::pair<map_iterator, bool> result;
        result = data_.insert(std::make_pair(key, v));
        return (*result.first).second;
    }

    bool has(Key const& key) const
    {
        return data_.find(key) != data_.end();
    }

    std::size_t size() const
    {
        return data_.size();
    }

    map_iterator begin() const
    {
        return data_.begin();
    }

    map_iterator end() const
    {
        return data_.end();
    }

    T& operator[](Key const& key)
    {
        return (*((data_.insert(std::make_pair(key, T()))).first)).second;
    }
};

template<class T> class ReadOnlyProperty
{
private:
    T value_;

public:
    template<typename... Args> ReadOnlyProperty(Args&&... args) : value_(args...) {}
    T& operator()() const { return value_; }
    T& get() const { return value_; }
    T *operator->() const { return &value_; }

    typedef T value_type;
};

//@}

} // namespace x0

#endif
