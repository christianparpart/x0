// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_SocketSpec_h
#define sw_x0_SocketSpec_h

#include <x0/Api.h>
#include <x0/IPAddress.h>
#include <functional>   // hash<>
#include <string>

namespace x0 {

class X0_API SocketSpec
{
public:
    enum Type {
        Unknown,
        Local,
        Inet,
    };

    SocketSpec();
    SocketSpec(const SocketSpec& ss);
    SocketSpec(const IPAddress& ipaddr, int port, int backlog = -1, size_t maccept = 1, bool reusePort = false) :
        type_(Inet),
        ipaddr_(ipaddr),
        port_(port),
        backlog_(backlog),
        multiAcceptCount_(maccept),
        reusePort_(reusePort)
    {}

    static SocketSpec fromString(const std::string& value);
    static SocketSpec fromLocal(const std::string& path, int backlog = -1);
    static SocketSpec fromInet(const IPAddress& ipaddr, int port, int backlog = -1);

    void clear();

    Type type() const { return type_; }
    bool isValid() const { return type_ != Unknown; }
    bool isLocal() const { return type_ == Local; }
    bool isInet() const { return type_ == Inet; }

    const IPAddress& ipaddr() const { return ipaddr_; }
    int port() const { return port_; }
    const std::string& local() const { return local_; }
    int backlog() const { return backlog_; }
    size_t multiAcceptCount() const { return multiAcceptCount_; }
    bool reusePort() const { return reusePort_; }

    void setPort(int value);
    void setBacklog(int value);
    void setMultiAcceptCount(size_t value);
    void setReusePort(bool value);

    std::string str() const;

private:
    Type type_;
    IPAddress ipaddr_;
    std::string local_;
    int port_;
    int backlog_;
    size_t multiAcceptCount_;
    bool reusePort_;
};


X0_API bool operator==(const x0::SocketSpec& a, const x0::SocketSpec& b);
X0_API bool operator!=(const x0::SocketSpec& a, const x0::SocketSpec& b);

inline X0_API bool operator==(const x0::SocketSpec& a, const x0::SocketSpec& b)
{
    if (a.type() != b.type())
        return false;

    switch (a.type()) {
        case x0::SocketSpec::Local:
            return a.local() == b.local();
        case x0::SocketSpec::Inet:
            return a.port() == b.port() && a.ipaddr() == b.ipaddr();
        default:
            return false;
    }
}

inline X0_API bool operator!=(const x0::SocketSpec& a, const x0::SocketSpec& b)
{
    return !(a == b);
}
} // namespace x0

namespace std
{
    template <>
    struct hash<x0::SocketSpec> :
        public unary_function<x0::SocketSpec, size_t>
    {
        size_t operator()(const x0::SocketSpec& v) const
        {
            switch (v.type()) {
                case x0::SocketSpec::Inet:
                    return hash<size_t>()(v.type()) ^ hash<x0::IPAddress>()(v.ipaddr());
                case x0::SocketSpec::Local:
                    return hash<size_t>()(v.type()) ^ hash<string>()(v.local());
                default:
                    return 0;
            }
        }
    };
}

#endif
