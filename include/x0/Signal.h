// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef x0_signal_hpp
#define x0_signal_hpp

#include <x0/Types.h>
#include <x0/Api.h>
#include <list>
#include <algorithm>
#include <functional>

namespace x0 {

//! \addtogroup base
//@{

/**
 * multi channel signal API.
 *
 * ...
 */
template<typename SignatureT> class Signal;

/**
 * \brief signal API
 * \see handler<void(Args...)>
 */
template<typename... Args>
class Signal<void(Args...)>
{
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;

public:
    typedef std::list<std::function<void(Args...)>> list_type;

    typedef typename list_type::iterator iterator;
    typedef typename list_type::const_iterator const_iterator;

    typedef iterator Connection;

public:
    Signal() :
        listeners_()
    {
    }

    ~Signal()
    {
    }

    /**
     * Tests whether this signal contains any listeners.
     */
    bool empty() const
    {
        return listeners_.empty();
    }

    /**
     * Retrieves the number of listeners to this signal.
     */
    std::size_t size() const
    {
        return listeners_.size();
    }

    /**
     * Connects a listener with this signal.
     */
    template<class K, void (K::*method)(Args...)>
    Connection connect(K *object)
    {
        return connect([=](Args... args) {
            (static_cast<K *>(object)->*method)(args...);
        });
    }

    /**
     * Connects a listener with this signal.
     */
    Connection connect(std::function<void(Args...)>&& cb)
    {
        listeners_.push_back(std::move(cb));
        auto handle = listeners_.end();
        --handle;
        return handle;
    }

    /**
     * Disconnects a listener from this signal.
     */
    void disconnect(Connection c)
    {
        listeners_.erase(c);
    }

    /**
     * Triggers this signal and notifies all listeners via their registered callback each with the given arguments passed.
     */
    void operator()(Args... args) const
    {
        for (auto listener: listeners_) {
            listener(args...);
        }
    }

    /**
     * Clears all listeners to this signal.
     */
    void clear()
    {
        listeners_.clear();
    }

private:
    list_type listeners_;
};

//@}

} // namespace x0

#endif
