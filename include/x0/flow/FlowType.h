#pragma once

#include <x0/Api.h>
#include <x0/Buffer.h>
#include <string>
#include <vector>
#include <cstdint>

namespace x0 {

//! \addtogroup Flow
//@{

enum class FlowType {
	Void = 0,
	Boolean = 1,        // bool (int64)
	Number = 2,         // int64
	String = 3,         // BufferRef*
	IPAddress = 5,      // IPAddress*
	Cidr = 6,           // Cidr*
	RegExp = 7,         // RegExp*
	Handler = 8,        // bool (*native_handler)(FlowContext*);
    IntArray = 9,       // array<int>
    StringArray = 10,   // array<string>
};

typedef uint64_t Register; // FlowVM

typedef int64_t FlowNumber;
typedef BufferRef FlowString;

std::string tos(FlowType type);

// {{{ array types
class X0_API FlowArray {
public:
    size_t size() const { return base_[0]; }

protected:
    explicit FlowArray(const Register* base) : base_(base) {}

    Register getRawAt(size_t i) const { return base_[1 + i]; }
    const Register* data() const { return base_ + 1; }

protected:
    const Register* base_;
};

class X0_API FlowIntArray : public FlowArray {
public:
    explicit FlowIntArray(const Register* base) : FlowArray(base) {}

    FlowNumber at(size_t i) const { return getRawAt(i); }
    FlowNumber operator[](size_t i) const { return getRawAt(i); }

    class iterator { // {{{
    private:
        const Register* current_;
        const Register* end_;

    public:
        iterator(const Register* beg, const Register* end) : current_(beg), end_(end) {}
        iterator(const iterator& v) = default;

        FlowNumber operator*() const {
            assert(current_ != end_);
            return *current_;
        }

        iterator& operator++() {
            if (current_ != end_) {
                ++current_;
            }
            return *this;
        }

        bool operator==(const iterator& other) const {
            return current_ == other.current_;
        }

        bool operator!=(const iterator& other) const {
            return current_ != other.current_;
        }
    }; // }}}

    iterator begin() const { return iterator(data(), data() + size()); }
    iterator end() const { return iterator(data() + size(), data() + size()); }
};

class X0_API FlowStringArray : public FlowArray {
public:
    explicit FlowStringArray(const Register* base) : FlowArray(base) {}

    const FlowString& at(size_t i) const { return *reinterpret_cast<const FlowString*>(getRawAt(i)); }
    const FlowString& operator[](size_t i) const { return *reinterpret_cast<const FlowString*>(getRawAt(i)); }

    class iterator { // {{{
    private:
        const Register* current_;
        const Register* end_;

    public:
        iterator(const Register* beg, const Register* end) : current_(beg), end_(end) {}
        iterator(const iterator& v) = default;

        const FlowString& operator*() const {
            assert(current_ != end_);
            return *reinterpret_cast<const FlowString*>(*current_);
        }

        iterator& operator++() {
            if (current_ != end_) {
                ++current_;
            }
            return *this;
        }

        bool operator==(const iterator& other) const {
            return current_ == other.current_;
        }

        bool operator!=(const iterator& other) const {
            return current_ != other.current_;
        }
    }; // }}}

    iterator begin() const { return iterator(data(), data() + size()); }
    iterator end() const { return iterator(data() + size(), data() + size()); }
};
// }}}

//!@}

} // namespace x0

namespace std {
    template<> struct hash<x0::FlowType> {
        uint32_t operator()(x0::FlowType v) const noexcept { return static_cast<uint32_t>(v); }
    };
}
