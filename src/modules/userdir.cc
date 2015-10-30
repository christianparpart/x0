// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "userdir.h"
#include "XzeroContext.h"
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/logging.h>
#include <sstream>

#include <sys/types.h>
#include <pwd.h>

using namespace xzero;
using namespace xzero::http;

namespace x0d {

UserdirModule::UserdirModule(XzeroDaemon* d)
    : XzeroModule(d, "compress"),
      dirname_("public_html") {

  setupFunction("userdir.name", &UserdirModule::userdir_name, flow::FlowType::String);
  mainFunction("userdir", &UserdirModule::userdir);
}

UserdirModule::~UserdirModule() {
}

void UserdirModule::userdir_name(Params& args) {
  std::string dirname = args.getString(1).str();

  std::error_code ec = validate(dirname);
  if (ec) {
    logError("x0d", "userdir \"$0\": $1", dirname, ec.message());
    return;
  }

  dirname_ = dirname;
}

std::error_code UserdirModule::validate(std::string& path) {
  if (path.empty())
    return std::make_error_code(std::errc::invalid_argument);

  if (path[0] == '/')
    return std::make_error_code(std::errc::invalid_argument);

  path.insert(0, "/");

  if (path[path.size() - 1] == '/')
    path = path.substr(0, path.size() - 1);

  return std::error_code();
}

void UserdirModule::userdir(XzeroContext* cx, Params& args) {
  if (dirname_.empty())
    return;

  auto requestPath = cx->request()->path();

  if (requestPath.size() <= 2 || requestPath[1] != '~')
    return;

  const std::size_t i = requestPath.find("/", 2);
  std::string userName, userPath;

  if (i != std::string::npos) {
    userName = requestPath.substr(2, i - 2);
    userPath = requestPath.substr(i);
  } else {
    userName = requestPath.substr(2);
    userPath = "";
  }

  if (struct passwd* pw = getpwnam(userName.c_str())) {
    cx->setDocumentRoot(pw->pw_dir + dirname_);
    cx->setFile(daemon().vfs().getFile(userPath, cx->documentRoot()));

    logTrace("x0d", "docroot[$0], fileinfo[$1]",
             cx->documentRoot(),
             cx->file()->path());
  }
}

} // namespace x0d
