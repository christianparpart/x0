// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "XzeroModule.h"
#include "XzeroDaemon.h"
#include "sysconfig.h"

#include <xzero-flow/ASTPrinter.h>
#include <xzero/logging/ConsoleLogTarget.h>
#include <xzero/logging.h>
#include <xzero/cli/CLI.h>
#include <xzero/cli/Flags.h>
#include <xzero/Application.h>
#include <iostream>
#include <unistd.h>

#define PACKAGE_VERSION X0_VERSION
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
  try {
    Application::init();

    CLI cli;
    cli.defineBool("help", 'h', "Prints this help and terminates.")
       .defineString("config", 'c', "PATH", "Specify a custom configuration file.", "/etc/x0d/x0d.conf", nullptr)
       .defineString("user", 'u', "NAME", "User privileges to drop down to.", Application::userName())
       .defineString("group", 'g', "NAME", "Group privileges to drop down to.", Application::groupName())
       .defineString("log-level", 'L', "ENUM", "Defines the minimum log level.", "info", nullptr)
       .defineString("log-target", 0, "ENUM", "Specifies logging target. One of syslog, file, systemd, console.", "console", nullptr)
       .defineString("log-file", 'l', "PATH", "Path to application log file.", "", nullptr)
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

    // {{{ setup logging
    Logger::get()->setMinimumLogLevel(to_loglevel(flags.getString("log-level")));

    std::string logTarget = flags.getString("log-target");
    if (logTarget == "null") {
      ; // ignore
    } else if (logTarget == "console") {
      Logger::get()->addTarget(ConsoleLogTarget::get());
    } else if (logTarget == "file") {
      ; // TODO
    } else if (logTarget == "syslog") {
      ; // TODO Logger::get()->addTarget(SyslogTarget::get());
    } else if (logTarget == "systemd") {
      ; // TODO Logger::get()->addTarget(SystemdLogTarget::get());
    } else {
      fprintf(stderr, "Invalid log target \"%s\".\n", logTarget.c_str());
      return 1;
    }
    // }}}

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
  } catch (const std::exception& e) {
    fprintf(stderr, "Unhandled exception caught. %s\n", e.what());
    return 1;
  }

  return 0;
}
