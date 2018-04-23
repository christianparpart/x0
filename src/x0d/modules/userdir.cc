// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "userdir.h"
#include <x0d/Context.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>
#include <sstream>

#if defined(XZERO_OS_UNIX)
#include <sys/types.h>
#include <pwd.h>
#endif

using namespace xzero;
using namespace xzero::http;

namespace x0d {

Result<std::string> getUserHomeDirectory(const std::string& userName) {
#if defined(XZERO_OS_WINDOWS)
  return std::errc::not_supported;
#else
  if (struct passwd* pw = getpwnam(userName.c_str()))
    return Success(std::string(pw->pw_dir));

  return std::make_error_code(static_cast<std::errc>(errno));
#endif
}

UserdirModule::UserdirModule(Daemon* d)
    : Module(d, "compress"),
      dirname_("public_html") {

  setupFunction("userdir.name", &UserdirModule::userdir_name, flow::LiteralType::String);
  mainFunction("userdir", &UserdirModule::userdir);
}

UserdirModule::~UserdirModule() {
}

void UserdirModule::userdir_name(Params& args) {
  std::string dirname = args.getString(1);

  std::error_code ec = validate(dirname);
  if (ec) {
    logError("userdir \"{}\": {}", dirname, ec.message());
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

void UserdirModule::userdir(Context* cx, Params& args) {
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

  Result<std::string> userdir = getUserHomeDirectory(userName);
  if (userdir) {
    std::string docroot = FileUtil::joinPaths(userdir.value(), dirname_);
    std::string filepath = FileUtil::joinPaths(docroot, userPath);

    cx->setDocumentRoot(docroot);
    cx->setFile(daemon().vfs().getFile(filepath));

    logTrace("docroot[{}], fileinfo[{}]",
             cx->documentRoot(),
             cx->file()->path());
  }
}

} // namespace x0d
