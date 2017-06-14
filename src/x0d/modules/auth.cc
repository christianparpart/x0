// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

/* plugin type: basic authentication
 *
 * description:
 *     Implements HTTP Basic Auth
 *
 * setup API:
 *     none
 *
 * request processing API:
 *     function auth.realm(string text);
 *     function auth.userfile(string path);
 *     function auth.ldap_user(string ldap_url[, string binddn, string bindpw])
 *     function auth.ldap_group(string ldap_url[, string binddn, string bindpw])
 *     handler auth.require();
 */

#include "auth.h"
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/base64.h>
#include <xzero/StringUtil.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>
#include <fstream>

#if defined(HAVE_SECURITY_PAM_APPL_H)
#include <security/pam_appl.h>
#endif

using namespace xzero;
using namespace xzero::http;
using namespace xzero::flow;

namespace x0d {

class AuthBackend { // {{{
 public:
  virtual ~AuthBackend() {}

  virtual bool authenticate(const std::string& username,
                            const std::string& passwd) = 0;
};
// }}}
// {{{ PAM support
#if defined(HAVE_SECURITY_PAM_APPL_H)
class AuthPAM : public AuthBackend {
 public:
  explicit AuthPAM(const std::string& service);
  virtual ~AuthPAM();

  bool authenticate(const std::string& username,
                    const std::string& passwd) override;

 private:
  static int callback(int num_msg, const struct pam_message** msg,
                      struct pam_response** resp, void* appdata_ptr);

  std::string service_;
  pam_conv conv_;
  pam_response* response_;
  std::string username_;
  std::string password_;
};

AuthPAM::AuthPAM(const std::string& service) {
  service_ = service;
  response_ = nullptr;
  conv_.conv = AuthPAM::callback;
  conv_.appdata_ptr = this;
}

AuthPAM::~AuthPAM() {
  free(response_);
}

bool AuthPAM::authenticate(const std::string& username,
                           const std::string& passwd) {
  username_ = username;
  password_ = passwd;

  pam_handle_t* pam = nullptr;
  int rv;

  rv = pam_start(service_.c_str(), username_.c_str(), &conv_, &pam);
  if (rv != PAM_SUCCESS) goto done;

  rv = pam_authenticate(pam, 0);
  if (rv != PAM_SUCCESS) goto done;

  rv = pam_acct_mgmt(pam, 0);

done:
  pam_end(pam, rv);
  return rv == PAM_SUCCESS;
}

int AuthPAM::callback(int num_msg, const struct pam_message** msg,
                      struct pam_response** resp, void* appdata_ptr) {
  AuthPAM* self = (AuthPAM*)appdata_ptr;

  if (self->response_) {
    free(self->response_);
    self->response_ = nullptr;
  }

  pam_response* response =
      (pam_response*)malloc(num_msg * sizeof(pam_response));
  if (!response) return PAM_CONV_ERR;

  for (int i = 0; i < num_msg; ++i) {
    response[i].resp_retcode = 0;
    switch (msg[i]->msg_style) {
      case PAM_PROMPT_ECHO_ON:
        response[i].resp = strdup(self->username_.c_str());  // free()'d by PAM
        break;
      case PAM_PROMPT_ECHO_OFF:
        response[i].resp = strdup(self->password_.c_str());  // free()'d by PAM
        break;
      case PAM_ERROR_MSG:
        // should display an error message
        break;
      case PAM_TEXT_INFO:
        // should display some informational text
        break;
      default:
        free(response);
        return PAM_CONV_ERR;
    }
  }

  *resp = response;
  self->response_ = response;  // so we can free it upon completion
  return PAM_SUCCESS;
}
#endif
// }}}
class AuthUserFile : public AuthBackend {// {{{
 private:
  std::string filename_;
  std::unordered_map<std::string, std::string> users_;

 public:
  explicit AuthUserFile(const std::string& filename);
  ~AuthUserFile();

  bool authenticate(const std::string& username,
                    const std::string& passwd) override;

 private:
  bool readFile();
};
// }}}
// {{{ AuthUserFile impl
AuthUserFile::AuthUserFile(const std::string& filename)
    : filename_(filename),
      users_() {
}

AuthUserFile::~AuthUserFile() {
}

bool AuthUserFile::readFile() {
  char buf[4096];
  std::ifstream in(filename_);
  users_.clear();

  while (in.good()) {
    in.getline(buf, sizeof(buf));
    size_t len = in.gcount();
    if (!len) continue;           // skip empty lines
    if (buf[0] == '#') continue;  // skip comments
    char* p = strchr(buf, ':');
    if (!p) continue;  // invalid line
    std::string name(buf, p - buf);
    std::string pass(1 + p, len - (p - buf) - 2);
    users_[name] = pass;
  }

  return !users_.empty();
}

bool AuthUserFile::authenticate(const std::string& username,
                                const std::string& passwd) {
  if (!readFile())
    return false;

  auto i = users_.find(username);
  if (i != users_.end())
    return i->second == passwd;

  return false;
}
// }}}
struct AuthBasic : public CustomData {  // {{{
  std::string realm;
  std::shared_ptr<AuthBackend> backend;

  AuthBasic() : realm("Restricted Area"), backend() {}

  ~AuthBasic() {}

  void setupUserfile(const std::string& userfile) {
    backend.reset(new AuthUserFile(userfile));
  }

#if defined(HAVE_SECURITY_PAM_APPL_H)
  void setupPAM(const std::string& service) {
    backend.reset(new AuthPAM(service));
  }
#endif

  bool verify(const char* user, const char* pass) {
    return backend->authenticate(user, pass);
  };
};
// }}}
// {{{ AuthModule
AuthModule::AuthModule(x0d::XzeroDaemon* d)
    : XzeroModule(d, "auth") {

  mainFunction("auth.realm", &AuthModule::auth_realm, flow::FlowType::String);
  mainFunction("auth.userfile", &AuthModule::auth_userfile, flow::FlowType::String);

#if defined(HAVE_SECURITY_PAM_APPL_H)
  mainFunction("auth.pam", &AuthModule::auth_pam, flow::FlowType::String);
#endif

  mainHandler("auth.require", &AuthModule::auth_require);
}

AuthModule::~AuthModule() {
}

void AuthModule::auth_realm(XzeroContext* cx, flow::vm::Params& args) {
  if (!cx->customData<AuthBasic>(this))
    cx->setCustomData<AuthBasic>(this);

  cx->customData<AuthBasic>(this)->realm = args.getString(1).str();
}

void AuthModule::auth_userfile(XzeroContext* cx, flow::vm::Params& args) {
  if (!cx->customData<AuthBasic>(this))
    cx->setCustomData<AuthBasic>(this);

  cx->customData<AuthBasic>(this)->setupUserfile(args.getString(1).str());
}

#if defined(HAVE_SECURITY_PAM_APPL_H)
void AuthModule::auth_pam(XzeroContext* cx, flow::vm::Params& args) {
  if (!cx->customData<AuthBasic>(this))
    cx->setCustomData<AuthBasic>(this);

  cx->customData<AuthBasic>(this)->setupPAM(args.getString(1).str());
}
#endif

bool AuthModule::auth_require(XzeroContext* cx, flow::vm::Params& args) {
  AuthBasic* auth = cx->customData<AuthBasic>(this);
  if (!auth || !auth->backend) {
    logError("auth", "'auth.require()' used without specifying a backend");
    cx->response()->setStatus(HttpStatus::InternalServerError);
    cx->response()->completed();
    return true;
  }

  std::string authorization = cx->request()->headers().get("Authorization");
  if (authorization.empty())
    return sendAuthenticateRequest(cx, auth->realm);

  if (StringUtil::beginsWith(authorization, "Basic ")) {
    std::string authcode = authorization.substr(6);

    Buffer plain;
    plain.reserve(base64::decodeLength(authcode));
    plain.resize(base64::decode(authcode, plain.begin()));

    const char* user = plain.c_str();
    char* pass = strchr(const_cast<char*>(plain.c_str()), ':');
    if (!pass)
      return sendAuthenticateRequest(cx, auth->realm);
    *pass++ = '\0';

    cx->request()->setUserName(user);

    logTrace("x0d.auth", "auth.require: '$0' -> '$1'", authcode, plain.c_str());

    if (auth->verify(user, pass)) {
      // authentification succeed, so do not intercept request processing
      return false;
    }
  }

  // authentification failed, one way or the other
  return sendAuthenticateRequest(cx, auth->realm);
}

bool AuthModule::sendAuthenticateRequest(XzeroContext* cx, const std::string& realm) {
  char buf[1024];
  snprintf(buf, sizeof(buf), "Basic realm=\"%s\"", realm.c_str());

  cx->response()->setHeader("WWW-Authenticate", buf);
  cx->response()->setStatus(HttpStatus::Unauthorized);
  cx->response()->completed();

  return true;
}
// }}}

} // namespace x0d
