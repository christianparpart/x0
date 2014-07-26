// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

/* plugin type: mapper
 *
 * description:
 *     Maps request path to a local file within the user's home directory.
 *
 * setup API:
 *     void userdir.name(string);
 *
 * request processing API:
 *     void userdir();
 */

#include <x0d/XzeroPlugin.h>
#include <xzero/HttpServer.h>
#include <xzero/HttpRequest.h>
#include <xzero/HttpHeader.h>
#include <base/strutils.h>
#include <base/Types.h>
#include <pwd.h>

using namespace xzero;
using namespace flow;
using namespace base;

/**
 * \ingroup plugins
 * \brief implements automatic index file resolving, if mapped request path is a
 * path.
 */
class userdir_plugin : public x0d::XzeroPlugin {
 private:
  HttpServer::RequestHook::Connection c;
  std::string dirname_;

 public:
  userdir_plugin(x0d::XzeroDaemon* d, const std::string& name)
      : x0d::XzeroPlugin(d, name), dirname_("/public_html") {
    setupFunction("userdir.name", &userdir_plugin::setup_userdir,
                  flow::FlowType::String);
    mainFunction("userdir", &userdir_plugin::handleRequest);
  }

  ~userdir_plugin() {}

  void setup_userdir(flow::FlowVM::Params& args) {
    std::string dirname = args.getString(1).str();

    std::error_code ec = validate(dirname);
    if (ec) {
      server().log(Severity::error, "userdir \"%s\": %s", dirname.c_str(),
                   ec.message().c_str());
      return;
    }

    dirname_ = dirname;
  }

  std::error_code validate(std::string& path) {
    if (path.empty()) return std::make_error_code(std::errc::invalid_argument);

    if (path[0] == '/')
      return std::make_error_code(std::errc::invalid_argument);

    path.insert(0, "/");

    if (path[path.size() - 1] == '/') path = path.substr(0, path.size() - 1);

    return std::error_code();
  }

 private:
  void handleRequest(HttpRequest* r, flow::FlowVM::Params& args) {
    if (dirname_.empty()) return;

    if (r->path.size() <= 2 || r->path[1] != '~') return;

    const std::size_t i = r->path.find("/", 2);
    std::string userName, userPath;

    if (i != std::string::npos) {
      userName = r->path.substr(2, i - 2);
      userPath = r->path.substr(i);
    } else {
      userName = r->path.substr(2);
      userPath = "";
    }

    if (struct passwd* pw = getpwnam(userName.c_str())) {
      r->documentRoot = pw->pw_dir + dirname_;
      r->fileinfo = r->connection.worker().fileinfo(r->documentRoot + userPath);
      // debug(0, "docroot[%s], fileinfo[%s]", r->documentRoot.c_str(),
      // r->fileinfo->filename().c_str());
    }
  }
};

X0D_EXPORT_PLUGIN(userdir)
