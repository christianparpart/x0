// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "XzeroModule.h"
#include "XzeroDaemon.h"

#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpOutput.h>
#include <xzero/http/HttpOutputCompressor.h>
#include <xzero/http/HttpFileHandler.h>
#include <xzero/http/http1/ConnectionFactory.h>
#include <xzero-flow/ASTPrinter.h>
#include <xzero/io/LocalFileRepository.h>
#include <xzero/executor/NativeScheduler.h>
#include <xzero/net/InetConnector.h>
#include <xzero/net/Server.h>
#include <xzero/logging/LogTarget.h>
#include <xzero/logging/ConsoleLogTarget.h>
#include <xzero/logging.h>
#include <xzero/cli/CLI.h>
#include <xzero/cli/Flags.h>
#include <xzero/RuntimeError.h>
#include <xzero/MimeTypes.h>
#include <xzero/WallClock.h>
#include <xzero/Application.h>
#include <xzero/StringUtil.h>
#include <iostream>
#include <unistd.h>

#define PACKAGE_VERSION "0.11.0-dev"
#define PACKAGE_HOMEPAGE_URL "https://xzero.io"

using namespace xzero;
using namespace xzero::http;

void printHelp(const CLI& cli) {
  std::cout 
    << "x0d: Xzero HTTP Web Server " PACKAGE_VERSION
        << " [" PACKAGE_HOMEPAGE_URL "]" << std::endl
    << "Copyright (c) 2009-2015 by Christian Parpart <trapni@gmail.com>" << std::endl
    << std::endl
    << "Usage: x0d [options ...]" << std::endl
    << std::endl
    << "Options:" << std::endl
    << cli.helpText() << std::endl;
}

#define DEFAULT_CONFIG_PATH PROJECT_ROOT_SRC_DIR "/x0d.conf"

int main(int argc, const char* argv[]) {
  Application::init();

  LogLevel logLevel = LogLevel::Warning;
  LogTarget* logTarget = nullptr;

  // {{{ env-var configuration
  if (const char* str = getenv("X0D_LOGLEVEL")) {
    logLevel = strToLogLevel(str);
  }

  if (const char* str = getenv("X0D_LOGTARGET")) {
    if (strcmp(str, "console") == 0) {
      logTarget = ConsoleLogTarget::get();
    } else if (strcmp(str, "syslog") == 0) {
      // TODO LogTarget::syslog()
    } else if (strncmp(str, "file:", 5) == 0) {
      // TODO LogTarget::file("filename")
    } else if (strcmp(str, "null") == 0) {
      // ignore
    } else {
      // invalid log target
    }
  } else {
    logTarget = ConsoleLogTarget::get();
  }
  // }}}

  Logger::get()->setMinimumLogLevel(logLevel);

  if (logTarget)
    Logger::get()->addTarget(logTarget);

  CLI cli;
  cli.defineBool("help", 'h', "Prints this help and terminates.")
     .defineString("config", 'c', "PATH", "Specify a custom configuration file.", "/etc/x0d/x0d.conf", nullptr)
     .defineString("user", 'u', "NAME", "User privileges to drop down to.", Application::userName())
     .defineString("group", 'g', "NAME", "Group privileges to drop down to.", Application::groupName())
     .defineString("instant", 'i', "PATH[:PORT]", "Enable instant-mode (does not need config file).", "", nullptr)
     .defineNumber("optimization-level", 'O', "LEVEL", "Sets the configuration optimization level.", 1)
     .defineBool("daemonize", 'd', "Forks the process into background.")
     .defineBool("dump-ast", 0, "Dumps configuration AST and exits.")
     .defineBool("dump-ir", 0, "Dumps configuration IR and exits.")
     .defineBool("dump-tc", 0, "Dumps configuration opcode stream and exits.")
     ;

  Flags flags = cli.evaluate(argc, argv);

  if (flags.getBool("help")) {
    printHelp(cli);
    return 0;
  }

  x0d::XzeroDaemon x0d;

  x0d.setOptimizationLevel(flags.getNumber("optimization-level"));

  x0d.loadConfigFile(flags.getString("config"));

  bool exitBeforeRun = false;

  if (flags.getBool("dump-ast")) {
    xzero::flow::ASTPrinter::print(x0d.programAST());
    exitBeforeRun = true;
  }

  if (flags.getBool("dump-ir")) {
    x0d.programIR()->dump();
    exitBeforeRun = true;
  }

  if (flags.getBool("dump-tc")) {
    x0d.program()->dump();
    exitBeforeRun = true;
  }

  if (exitBeforeRun)
    return 0;

  if (!x0d.configure())
    return 1;

  Application::dropPrivileges(flags.getString("user"), flags.getString("group"));

  if (flags.getBool("daemonize"))
    Application::daemonize();

  x0d.run();

  return 0;
}
