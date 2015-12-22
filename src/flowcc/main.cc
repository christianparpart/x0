// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <stdio.h>
#include <xzero/cli/CLI.h>
#include <xzero/cli/Flags.h>

namespace nginx { // {{{

class Upstream {};
class Listener {};
class RewriteRule {};
class LocationMatch {};
class ExactMatch : public LocationMatch {};
class RegexMatch : public LocationMatch {};
class PrefixMatch : public LocationMatch {};

class Server {
 private:
  std::vector<Listener> listeners_;
  std::vector<RewriteRule> rewriteRules_;
  std::vector<ExactMatch> exactMatches_;
  std::vector<RegexMatch> regexMatches_;
  std::vector<PrefixMatch> prefixMatches_;
};

class Global {
  std::vector<Server> servers_;
  // ...
};

} // namespace nginx }}}

class NginxFlow {
 public:
  int run(int argc, const char* argv[]);

 private:
  xzero::Flags flags_;
};

using namespace xzero;
using namespace xzero::flow;

int NginxFlow::run(int argc, const char* argv[]) {
  CLI cli;
  cli.defineBool("help", 'h', "Prints this help.", nullptr);
  cli.defineString("log-level", 'L', "LEVEL", "Defines minimum log level (error, warning, debug, trace).", nullptr);
  cli.defineString("output", 'o', "PATH", "Output file.", nullptr);
  cli.defineString("input-format", 's', "Input format (nginx, x0d, apache)", nullptr);
  cli.defineString("output-format", 't', "Input format (nginx, x0d)", nullptr);
  cli.enableParameters("PATH", "Path to nginx configuration file.");

  flags_ = cli.evaluate(argc, argv);

  return 0;
}

int main(int argc, const char* argv[]) {
  NginxFlow ngf;
  return ngf.run(argc, argv);
}
