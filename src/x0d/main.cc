// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroModule.h>
#include <x0d/XzeroDaemon.h>

#include <xzero/sysconfig.h>
#include <xzero/logging/ConsoleLogTarget.h>
#include <xzero/logging/FileLogTarget.h>
#include <xzero/logging.h>
#include <xzero/io/FileUtil.h>
#include <xzero/StringUtil.h>
#include <xzero/Application.h>
#include <xzero/Flags.h>
#include <typeinfo>
#include <iostream>
#include <memory>
#include <unistd.h>

using namespace xzero;
using namespace xzero::http;

void printVersion() {
  std::cout
    << "x0d: Xzero HTTP Web Server " PACKAGE_VERSION
        << " [" PACKAGE_URL "]" << std::endl
    << "Copyright (c) 2009-2017 by Christian Parpart <christian@parpart.family>" << std::endl;
}

void printHelp(const Flags& cli) {
  printVersion();
  std::cout
    << std::endl
    << "Usage: x0d [options ...]" << std::endl
    << std::endl
    << "Options:" << std::endl
    << cli.helpText()
    << std::endl;
}

class PidFile { // {{{
 public:
  PidFile(const std::string& path);
  ~PidFile();

 private:
  std::string path_;
};

PidFile::PidFile(const std::string& path)
    : path_(path) {
  // TODO: sanity-check (flock?) to ensure that we're the one.
  logInfo("x0d", "Writing main process ID $0 into file $1",
          getpid(), path_);
  FileUtil::write(path_, to_string(getpid()));
}

PidFile::~PidFile() {
  FileUtil::rm(path_);
}
// }}}

int main(int argc, const char* argv[]) {
  std::unique_ptr<FileLogTarget> fileLogTarget;
  Application::init();

  Flags flags;
  flags.defineBool("help", 'h', "Prints this help and exits.")
       .defineBool("version", 'v', "Prints software version and exits.")
       .defineBool("webfile", 'w', "Looks out for a Webfile in current working directory as configuration file and uses that instead.")
       .defineString("config", 'c', "PATH", "Specify a custom configuration file.", X0D_CONFIGFILE, nullptr)
       .defineString("user", 'u', "NAME", "User privileges to drop down to.", Application::userName())
       .defineString("group", 'g', "NAME", "Group privileges to drop down to.", Application::groupName())
       .defineString("log-level", 'L', "ENUM", "Defines the minimum log level.", "info", nullptr)
       .defineString("log-target", 0, "ENUM", "Specifies logging target. One of syslog, file, systemd, console.", "console", nullptr)
       .defineString("log-file", 'l', "PATH", "Path to application log file.", "", nullptr)
       .defineString("instant", 'i', "[BIND:]:PORT", "Enable instant-mode (does not need config file).", "", nullptr)
       .defineNumber("optimization-level", 'O', "LEVEL", "Sets the configuration optimization level.", 1)
       .defineBool("daemonize", 'd', "Forks the process into background.")
       .defineString("pid-file", 0, "PATH",
                     "Path to PID-file this process will store its main PID.",
                     X0D_PIDFILE,
                     nullptr)
       .defineBool("dump-ast", 0, "Dumps configuration AST and exits.")
       .defineBool("dump-ir", 0, "Dumps configuration IR and exits.")
       .defineBool("dump-tc", 0, "Dumps configuration opcode stream and exits.")
       ;

  std::error_code ec = flags.parse(argc, argv);
  if (ec) {
    fprintf(stderr, "Failed to parse flags. %s\n", ec.message().c_str());
    return 1;
  }

  if (flags.getBool("help")) {
    printHelp(flags);
    return 0;
  }

  if (flags.getBool("version")) {
    printVersion();
    return 0;
  }

  x0d::XzeroDaemon x0d;

  // {{{ setup logging
  Logger::get()->setMinimumLogLevel(make_loglevel(flags.getString("log-level")));

  std::string logTarget = flags.getString("log-target");
  if (logTarget == "null") {
    ; // ignore
  } else if (logTarget == "console") {
    Logger::get()->addTarget(ConsoleLogTarget::get());
  } else if (logTarget == "file") {
    std::string filename = flags.getString("log-file");
    std::shared_ptr<File> file = x0d.vfs().getFile(filename);
    File::OpenFlags openFlags = File::Write | File::Create | File::Append;
    FileDescriptor out = file->createPosixChannel(openFlags);
    fileLogTarget.reset(new FileLogTarget(std::move(out)));
    Logger::get()->addTarget(fileLogTarget.get());
  } else if (logTarget == "syslog") {
    ; // TODO Logger::get()->addTarget(SyslogTarget::get());
  } else if (logTarget == "systemd") {
    ; // TODO Logger::get()->addTarget(SystemdLogTarget::get());
  } else {
    fprintf(stderr, "Invalid log target \"%s\".\n", logTarget.c_str());
    return 1;
  }
  // }}}

  x0d.setOptimizationLevel(flags.getNumber("optimization-level"));

  bool dumpAST = flags.getBool("dump-ast");
  bool dumpIR = flags.getBool("dump-ir");
  bool dumpTC = flags.getBool("dump-tc");
  bool webfile = flags.getBool("webfile");

  std::string configFileName = webfile ? "Webfile"
                                       : flags.getString("config");

  if (webfile && flags.getString("config") != X0D_CONFIGFILE) {
    logError("x0d", "Do not use --webfile and --config options at once.");
    return 1;
  }

  std::shared_ptr<xzero::flow::vm::Program> config =
      x0d.loadConfigFile(configFileName, dumpAST, dumpIR, dumpTC);

  bool exitBeforeRun = dumpAST || dumpIR || dumpTC;
  if (exitBeforeRun)
    return 0;

  if (!x0d.applyConfiguration(config))
    return 1;

  std::string pidfilepath = FileUtil::absolutePath(flags.getString("pid-file"));
  std::string pidfiledir = FileUtil::dirname(pidfilepath);
  if (!FileUtil::exists(pidfiledir)) {
    FileUtil::mkdir_p(pidfiledir);
    FileUtil::chown(pidfiledir, flags.getString("user"),
                                flags.getString("group"));
  }

  Application::dropPrivileges(flags.getString("user"), flags.getString("group"));

  if (flags.getBool("daemonize")) {
    Application::daemonize();
  }

  PidFile pidFile = pidfilepath;

  x0d.run();
  return 0;
}
