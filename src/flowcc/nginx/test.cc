// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <list>
#include <string>

namespace nginx {
namespace ast {

class Directive {};
class UpstreamServer : public Directive { };
class UpstreamCluster : public Directive {
  std::list<UpstreamServerDef> servers;
};

class RewriteRule : public Directive { };

// contexts
class Context : public Directive {};
class Global : public Context {};
class LocationBlock : public Context { };
class RegexLocation : public LocationBlock { };
class ExactLocation : public LocationBlock { };
class PrefixLocation : public LocationBlock { };
class Server : public Context {
  std::list<Listen> listen;
  std::list<std::string> server_name;
  std::list<Variable> variables;

  Option<std::string> ssl_certificate;
  Option<std::string> ssl_certificate_key;
  Option<std::string> root;

  std::list<RewriteRule> rewriteRules;
  std::list<RegexLocation> regexMatches;
  std::list<ExactLocation> exactMatches;
  std::list<PrefixLocation> prefixMatches;
};

} // namespace ast

void NginxToXzero::listen(Context* cx, const std::list<std::string>& params) {
  // ...
}

} // namespace nginx
