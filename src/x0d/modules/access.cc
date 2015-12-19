// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <x0d/modules/access.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/net/IPAddress.h>

using namespace xzero;
using namespace xzero::http;

namespace x0d {

AccessModule::AccessModule(x0d::XzeroDaemon* d)
    : XzeroModule(d, "access") {

  mainHandler("access.deny", &AccessModule::deny_all);
  mainHandler("access.deny", &AccessModule::deny_ip, FlowType::IPAddress);
  mainHandler("access.deny", &AccessModule::deny_cidr, FlowType::Cidr);
  mainHandler("access.deny", &AccessModule::deny_ipArray, FlowType::IPAddrArray);
  mainHandler("access.deny", &AccessModule::deny_cidrArray, FlowType::CidrArray);

  mainHandler("access.deny_except", &AccessModule::denyExcept_ip, FlowType::IPAddress);
  mainHandler("access.deny_except", &AccessModule::denyExcept_cidr, FlowType::Cidr);
  mainHandler("access.deny_except", &AccessModule::denyExcept_ipArray, FlowType::IPAddrArray);
  mainHandler("access.deny_except", &AccessModule::denyExcept_cidrArray, FlowType::CidrArray);
}

// {{{ deny()
bool AccessModule::deny_all(XzeroContext* cx, Params& args) {
  return forbidden(cx);
}

bool AccessModule::deny_ip(XzeroContext* cx, Params& args) {
  if (cx->remoteIP() == args.getIPAddress(1))
    return forbidden(cx);

  return false;
}

bool AccessModule::deny_cidr(XzeroContext* cx, Params& args) {
  if (args.getCidr(1).contains(cx->remoteIP()))
    return forbidden(cx);

  return false;
}

bool AccessModule::deny_ipArray(XzeroContext* cx, Params& args) {
  const auto& list = args.getIPAddressArray(1);
  for (const auto& ip : list)
    if (ip == cx->remoteIP())
      return forbidden(cx);

  return false;
}

bool AccessModule::deny_cidrArray(XzeroContext* cx, Params& args) {
  const auto& list = args.getCidrArray(1);
  for (const auto& cidr : list)
    if (cidr.contains(cx->remoteIP()))
      return forbidden(cx);

  return false;
}
// }}}
// {{{ deny_except()
bool AccessModule::denyExcept_ip(XzeroContext* cx, Params& args) {
  if (cx->remoteIP() == args.getIPAddress(1))
    return false;

  return forbidden(cx);
}

bool AccessModule::denyExcept_cidr(XzeroContext* cx, Params& args) {
  if (args.getCidr(1).contains(cx->remoteIP()))
    return false;

  return forbidden(cx);
}

bool AccessModule::denyExcept_ipArray(XzeroContext* cx, Params& args) {
  const auto& list = args.getIPAddressArray(1);
  for (const auto& ip : list)
    if (ip == cx->remoteIP())
      return false;

  return forbidden(cx);
}

bool AccessModule::denyExcept_cidrArray(XzeroContext* cx, Params& args) {
  const auto& list = args.getCidrArray(1);
  for (const auto& cidr : list)
    if (cidr.contains(cx->remoteIP()))
      return false;

  return forbidden(cx);
}
// }}}

bool AccessModule::forbidden(XzeroContext* cx) {
  cx->response()->setStatus(HttpStatus::Forbidden);
  cx->response()->completed();

  return true;
}

} // namespace x0d
