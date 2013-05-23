/* <x0/Queue.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <x0/Defines.h>

namespace x0 {

#if defined(__x86_64__) || defined(_M_X64)

/*! Embeds an integer tag into a pointer.
 */
template<typename T>
class TaggedPtr
{
public:
	typedef T* pointer_type;
	typedef T& reference_type;
	typedef uint16_t tag_type;

	TaggedPtr() : ptr_(pack(nullptr, 0)) {}
	TaggedPtr(pointer_type p, tag_type t = tag_type()) : ptr_(pack(p, t)) {}
	TaggedPtr(const TaggedPtr&) = default;

	void set(pointer_type p, tag_type t) { ptr_ = pack(p, t); }

	constexpr pointer_type get() const { return ptr(); }

	constexpr pointer_type ptr() const volatile { return pointer_type(ptr_ & ((1llu << 48) - 1)); }
	constexpr tag_type tag() const volatile { return (ptr_ >> 48) & ((1llu << 16) - 1); }

	pointer_type operator->() const { return ptr(); }
	reference_type operator*() const { return *ptr(); }

	constexpr operator bool () const { return ptr() != nullptr; }
	constexpr bool operator!() const { return ptr() == nullptr; }

	constexpr bool operator==(const TaggedPtr& other) const volatile { return ptr() == other.ptr() && tag() == other.tag(); }
	constexpr bool operator!=(const TaggedPtr& other) const volatile { return !(*this == other); }

private:
	static constexpr uint64_t pack(pointer_type p, tag_type t)
	{
		return ((uint64_t(t) & ((1llu << 16) - 1)) << 48)
			  | (uint64_t(p) & ((1llu << 48) - 1));
	}

	uint64_t ptr_;
};

#else

template<typename T>
struct TaggedPtr
{
public:
	typedef size_t tag_type;
	typedef T* pointer_type;
	typedef T& reference_type;

private:
	pointer_type ptr_;
	tag_type tag_;

public:
	TaggedPtr() : ptr_(nullptr), tag_(0) {}
	TaggedPtr(pointer_type ptr, tag_type tag) : ptr_(ptr), tag_(tag) {}
	TaggedPtr(const TaggedPtr&) = default;

	void set(pointer_type p, tag_type t) volatile { ptr_ = p; tag_ = t; }

	constexpr pointer_type get() const volatile { return ptr_; }
	constexpr pointer_type ptr() const volatile { return ptr_; }
	constexpr tag_type tag() const volatile { return tag_; }

	constexpr operator bool () const { return ptr() != nullptr; }
	constexpr bool operator!() const { return ptr() == nullptr; }

	constexpr pointer_type operator->() const { return ptr(); }
	constexpr reference_type operator*() const { return *ptr(); }

	constexpr bool operator==(volatile const TaggedPtr<T>& other) const { return ptr_ == other.ptr_ && tag_ == other.tag_; }
	constexpr bool operator!=(volatile const TaggedPtr<T>& other) const { return !operator==(other); }
};

#endif

} // namespace x0
