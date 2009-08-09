/* <x0/property.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_property_h
#define x0_property_h

// C++ template based properties, as defined in: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2004/n1615.pdf

#include <map>
#include <boost/function.hpp>

namespace x0 {

template<class T>
class value_property
{
private:
	T value_;

public:
	value_property() : value_() {}
	value_property(T const& v) : value_(v) {}

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

	T operator+=(const T& v)
	{
		value_ += v;
		return value_;
	}

	typedef T value_type;
};

/** read-only value property. */
template<class T>
class rvalue_read_property
{
private:
	const T& value_;

public:
	template<class U>
	explicit rvalue_read_property(U v) : value_(v) {}

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
class read_property
{
private:
	Object *object_;

public:
	read_property(Object *obj) : object_(obj)
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
	T (Object::*real_set)(T const&)
>
class write_property
{
private:
	Object *object_;

public:
	write_property(Object *obj) : object_(obj)
	{
	}

	write_property(Object *obj, T const & v) : object_(obj)
	{
		(object_->real_set)(v);
	}

	void operator()(Object *obj)
	{
		object_ = obj;
	}

	T operator()(T const& value)
	{
		return (object_->*real_set)(value);
	}

	T set(T const& value)
	{
		return (object_->*real_set)(value);
	}

	T operator=(T const& value)
	{
		return (object_->*real_set)(value);
	}

	typedef T value_type;
};

template<typename T>
class property
{
private:
	boost::function<T()> get_;
	boost::function<T(const T&)> set_;

public:
	template<class Getter, class Setter>
	property(Getter _get, Setter _set) :
		get_(_get), set_(_set)
	{
	}

	template<class Getter, class Setter>
	property(Getter _get, Setter _set, const T& v) :
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
class indexed_property
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

template<class T> class __ro_prop {
private:
	T value_;

public:
	template<typename... Args> __ro_prop(Args... args) : value_(args...) {}
	T& operator()() const { return value_; }
	T& get() const { return value_; }
	T *operator->() const { return &value_; }
};

} // namespace x0

#endif
