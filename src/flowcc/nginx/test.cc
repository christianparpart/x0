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
