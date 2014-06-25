// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0/http/HttpHeader.h>
#include <x0/Tokenizer.h>
#include <x0/Api.h>
#include <memory>
#include <vector>
#include <string>

namespace x0 {

class HttpRequest;

class X0_API HttpVary
{
public:
    enum class Match {
        None,
        ValuesDiffer,
        Equals,
    };

    explicit HttpVary(size_t count);
    ~HttpVary();

    HttpVary(const HttpVary&) = delete;
    HttpVary& operator=(const HttpVary&) = delete;

    size_t size() const;

    const std::vector<BufferRef>& names() const;
    const std::vector<BufferRef>& values() const;

    Match match(const x0::HttpRequest* r) const;
    Match match(const HttpVary& other) const;

    /**
     * Creates a HttpVary object, based on the Response request header
     *
     * @param vary HttpVary HTTP response header, a comma seperated list of request headers.
     * @param requestHeaders list of request headers
     *
     * @return a HttpVary instance reflecting the list of varying fields.
     */
    template<typename T, typename U>
    static std::unique_ptr<HttpVary> create(const U& vary, const std::vector<HttpHeader<T>>& requestHeaders);

    /**
     * Creates a HttpVary object, based on the Response request header
     */
    static std::unique_ptr<HttpVary> create(const x0::HttpRequest* r);

    class iterator {
    public:
        iterator() {}
        iterator(HttpVary* vary, size_t i) : 
            vary_(vary), i_(i), e_(vary->size()) {}

        iterator& operator++() {
            if (i_ != e_)
                ++i_;

            return *this;
        }

        bool operator==(const iterator& other) { return i_ == other.i_; }
        bool operator!=(const iterator& other) { return !(*this == other); }

        std::pair<const BufferRef&, const BufferRef&>  operator*() const {
            return std::make_pair(vary_->names()[i_], vary_->values()[i_]);
        }

        const BufferRef& name() const { return vary_->names()[i_]; }
        const BufferRef& value() const { return vary_->values()[i_]; }

    private:
        HttpVary* vary_;
        size_t i_;
        size_t e_;
    };

    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, size()); }

private:
    template<typename T, typename U>
    static inline T find(const U& name, const std::vector<HttpHeader<T>>& requestHeaders);

private:
    std::vector<BufferRef> names_;
    std::vector<BufferRef> values_;
};

// {{{ inlines
inline size_t HttpVary::size() const
{
    return names_.size(); // this is okay since names.size() always equals values.size()
}

inline const std::vector<BufferRef>& HttpVary::names() const
{
    return names_;
}

inline const std::vector<BufferRef>& HttpVary::values() const
{
    return values_;
}

template<typename T, typename U>
inline T HttpVary::find(const U& name, const std::vector<HttpHeader<T>>& requestHeaders)
{
    for (auto& i: requestHeaders)
        if (iequals(name, i.name))
            return i.value;

    return T();
}

template<typename T, typename U>
std::unique_ptr<HttpVary> HttpVary::create(const U& varyHeader, const std::vector<HttpHeader<T>>& requestHeaders)
{
//	if (varyHeader.empty())
//		return std::unique_ptr<HttpVary>();

    auto tokens = Tokenizer<BufferRef>::tokenize(varyHeader, ", ");
    std::unique_ptr<HttpVary> vary(new HttpVary(tokens.size()));
    for (size_t i = 0, e = tokens.size(); i != e; ++i) {
        auto name = tokens[i];
        vary->names_[i] = name;
        vary->values_[i] = find(name, requestHeaders);
    }

    return vary;
}
// }}}

} // namespace x0
