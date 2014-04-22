/* <x0/plugins/access.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 *
 * --------------------------------------------------------------------------
 *
 * plugin type: basic access control
 *
 * description:
 *     Implements basic IP & CIDR network based Access Control
 *
 * setup API:
 *     none
 *
 * request processing API:
 *     handler access.deny();
 *
 *     handler access.deny(IP);
 *     handler access.deny(Cidr);
 *     handler access.deny(IP[]);
 *     handler access.deny(Cidr[]);
 *
 *     handler access.deny_except(IP);
 *     handler access.deny_except(Cidr);
 *     handler access.deny_except(IP[]);
 *     handler access.deny_except(Cidr[]);
 */

#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/Types.h>

using namespace x0;

class AccessPlugin :
    public x0d::XzeroPlugin
{
public:
    AccessPlugin(x0d::XzeroDaemon* d, const std::string& name) :
        x0d::XzeroPlugin(d, name)
    {
        mainHandler("access.deny", &AccessPlugin::deny_all);
        mainHandler("access.deny", &AccessPlugin::deny_ip, FlowType::IPAddress);
        mainHandler("access.deny", &AccessPlugin::deny_cidr, FlowType::Cidr);
        mainHandler("access.deny", &AccessPlugin::deny_ipArray, FlowType::IPAddrArray);
        mainHandler("access.deny", &AccessPlugin::deny_cidrArray, FlowType::CidrArray);

        mainHandler("access.deny_except", &AccessPlugin::deny_ip, FlowType::IPAddress);
        mainHandler("access.deny_except", &AccessPlugin::deny_cidr, FlowType::Cidr);
        mainHandler("access.deny_except", &AccessPlugin::deny_ipArray, FlowType::IPAddrArray);
        mainHandler("access.deny_except", &AccessPlugin::deny_cidrArray, FlowType::CidrArray);
    }

private:
    // {{{ deny()
    bool deny_all(HttpRequest* r, FlowVM::Params& args)
    {
        return forbidden(r);
    }

    bool deny_ip(HttpRequest* r, FlowVM::Params& args)
    {
        if (r->connection.remoteIP() == args.getIPAddress(1))
            return forbidden(r);

        return false;
    }

    bool deny_cidr(HttpRequest* r, FlowVM::Params& args)
    {
        if (args.getCidr(1).contains(r->connection.remoteIP()))
            return forbidden(r);

        return false;
    }

    bool deny_ipArray(HttpRequest* r, FlowVM::Params& args)
    {
        const auto& list = args.getIPAddressArray(1);
        for (const auto& ip: list)
            if (ip == r->connection.remoteIP())
                return forbidden(r);

        return false;
    }

    bool deny_cidrArray(HttpRequest* r, FlowVM::Params& args)
    {
        const auto& list = args.getCidrArray(1);
        for (const auto& cidr: list)
            if (cidr.contains(r->connection.remoteIP()))
                return forbidden(r);

        return false;
    }
    // }}}
    // {{{ deny_except()
    bool denyExcept_ip(HttpRequest* r, FlowVM::Params& args)
    {
        if (r->connection.remoteIP() == args.getIPAddress(1))
            return false;

        return forbidden(r);
    }

    bool denyExcept_cidr(HttpRequest* r, FlowVM::Params& args)
    {
        if (args.getCidr(1).contains(r->connection.remoteIP()))
            return false;

        return forbidden(r);
    }

    bool denyExcept_ipArray(HttpRequest* r, FlowVM::Params& args)
    {
        const auto& list = args.getIPAddressArray(1);
        for (const auto& ip: list)
            if (ip == r->connection.remoteIP())
                return false;

        return forbidden(r);
    }

    bool denyExcept_cidrArray(HttpRequest* r, FlowVM::Params& args)
    {
        const auto& list = args.getCidrArray(1);
        for (const auto& cidr: list)
            if (cidr.contains(r->connection.remoteIP()))
                return false;

        return forbidden(r);
    }
    // }}}

    bool forbidden(HttpRequest* r)
    {
        r->status = HttpStatus::Forbidden;
        r->finish();

        return true;
    }
};

X0_EXPORT_PLUGIN_CLASS(AccessPlugin)
