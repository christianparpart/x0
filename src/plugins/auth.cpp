// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
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

#include <x0d/XzeroPlugin.h>
#include <xzero/HttpServer.h>
#include <xzero/HttpRequest.h>
#include <xzero/HttpHeader.h>
#include <base/Base64.h>
#include <base/Types.h>
#include <x0d/sysconfig.h>
#include <fstream>

#if defined(HAVE_SECURITY_PAM_APPL_H)
#include <security/pam_appl.h>
#endif

using namespace xzero;
using namespace flow;
using namespace base;

class AuthBackend  // {{{
    {
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

  virtual bool authenticate(const std::string& username,
                            const std::string& passwd);

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

AuthPAM::~AuthPAM() { free(response_); }

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
class AuthUserFile :  // {{{
                      public AuthBackend {
 private:
  std::string filename_;
  std::unordered_map<std::string, std::string> users_;

 public:
  explicit AuthUserFile(const std::string& filename);
  ~AuthUserFile();

  virtual bool authenticate(const std::string& username,
                            const std::string& passwd);

 private:
  bool readFile();
};
// }}}
// {{{ AuthUserFile impl
AuthUserFile::AuthUserFile(const std::string& filename)
    : filename_(filename), users_() {}

AuthUserFile::~AuthUserFile() {}

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
  if (!readFile()) return false;

  auto i = users_.find(username);
  if (i != users_.end()) return i->second == passwd;

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
class AuthPlugin :  // {{{
                    public x0d::XzeroPlugin {
 public:
  AuthPlugin(x0d::XzeroDaemon* d, const std::string& name)
      : x0d::XzeroPlugin(d, name) {
    mainFunction("auth.realm", &AuthPlugin::auth_realm, flow::FlowType::String);
    mainFunction("auth.userfile", &AuthPlugin::auth_userfile,
                 flow::FlowType::String);

#if defined(HAVE_SECURITY_PAM_APPL_H)
    mainFunction("auth.pam", &AuthPlugin::auth_pam, flow::FlowType::String);
#endif

    mainHandler("auth.require", &AuthPlugin::auth_require);
  }

  ~AuthPlugin() {}

 private:
  void auth_realm(HttpRequest* r, flow::vm::Params& args) {
    if (!r->customData<AuthBasic>(this)) r->setCustomData<AuthBasic>(this);

    r->customData<AuthBasic>(this)->realm = args.getString(1).str();
  }

  void auth_userfile(HttpRequest* r, flow::vm::Params& args) {
    if (!r->customData<AuthBasic>(this)) r->setCustomData<AuthBasic>(this);

    r->customData<AuthBasic>(this)->setupUserfile(args.getString(1).str());
  }

#if defined(HAVE_SECURITY_PAM_APPL_H)
  void auth_pam(HttpRequest* r, flow::vm::Params& args) {
    if (!r->customData<AuthBasic>(this)) r->setCustomData<AuthBasic>(this);

    r->customData<AuthBasic>(this)->setupPAM(args.getString(1).str());
  }
#endif

  bool auth_require(HttpRequest* r, flow::vm::Params& args) {
    AuthBasic* auth = r->customData<AuthBasic>(this);
    if (!auth || !auth->backend) {
      r->log(Severity::error,
             "auth.require used without specifying a backend");
      r->status = HttpStatus::InternalServerError;
      r->finish();
      return true;
    }

    auto authorization = r->requestHeader("Authorization");
    if (!authorization) return sendAuthenticateRequest(r, auth->realm);

    if (authorization.begins("Basic ")) {
      BufferRef authcode = authorization.ref(6);
      Buffer plain = Base64::decode(authcode);
      const char* user = plain.c_str();
      char* pass = strchr(const_cast<char*>(plain.c_str()), ':');
      if (!pass) return sendAuthenticateRequest(r, auth->realm);
      *pass++ = '\0';

      r->username = user;

      r->log(Severity::trace, "auth.require: '%s' -> '%s'",
             authcode.str().c_str(), plain.c_str());

      if (auth->verify(user, pass)) {
        // authentification succeed, so do not intercept request processing
        return false;
      }
    }

    // authentification failed, one way or the other
    return sendAuthenticateRequest(r, auth->realm);
  }

  bool sendAuthenticateRequest(HttpRequest* r, const std::string& realm) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "Basic realm=\"%s\"", realm.c_str());
    r->responseHeaders.push_back("WWW-Authenticate", buf);
    r->status = HttpStatus::Unauthorized;
    r->finish();
    return true;
  }
};
// }}}

X0D_EXPORT_PLUGIN_CLASS(AuthPlugin)
