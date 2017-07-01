// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/Application.h>
#include <xzero/Buffer.h>
#include <xzero/StringUtil.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/SystemPipe.h>
#include <xzero/executor/Executor.h>
#include <xzero/thread/SignalHandler.h>
#include <xzero/logging/ConsoleLogTarget.h>
#include <xzero/logging/LogLevel.h>
#include <xzero/logging.h>
#include <xzero/RuntimeError.h>
#include <xzero/sysconfig.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <fcntl.h>

namespace xzero {

void Application::init() {
  Application::installGlobalExceptionHandler();

  thread::SignalHandler::ignore(SIGPIPE);

  // well, when you detach from the terminal, you're garanteed to not get one.
  // unless someone sends it explicitely (so why ignoring then)?
  thread::SignalHandler::ignore(SIGHUP);
}

std::string Application::appName() {
  return StringUtil::split(
      FileUtil::read(StringUtil::format("/proc/$0/cmdline", getpid())).str(),
      " ")[0];
}

void Application::logToStderr(LogLevel loglevel) {
  Logger::get()->setMinimumLogLevel(loglevel);
  Logger::get()->addTarget(ConsoleLogTarget::get());
}

void Application::redirectStdOutToLogger(Executor* executor) {
  static SystemPipe pipe;

  int ec = dup2(pipe.writerFd(), STDOUT_FILENO);
  if (ec < 0)
    RAISE_ERRNO(errno);

  int fd = pipe.readerFd();
  fcntl(fd, F_SETFL, O_NONBLOCK);

  executor->executeOnReadable(fd, [fd]() {
    fprintf(stderr, "[stdout]: received message\n");
    Buffer buf(4096);
    FileUtil::read(fd, &buf);
    if (buf.size() > 0) {
      fprintf(stderr, "[stdout] %s\n", buf.c_str());
      logInfo("stdout", "$0", buf.str());
      buf.clear();
    }
    fflush(stderr);
  });
}

void Application::redirectStdErrToLogger(Executor* executor) {
}

static void globalEH() {
  try {
    throw;
  } catch (const std::exception& e) {
    logAndAbort(e);
  } catch (...) {
    // d'oh
    fprintf(stderr, "Unhandled foreign exception caught.\n");
    abort();
  }
}

void Application::installGlobalExceptionHandler() {
  std::set_terminate(&globalEH);
  std::set_unexpected(&globalEH);
}

std::string Application::userName() {
  if (struct passwd* pw = getpwuid(getuid()))
    return pw->pw_name;

  RAISE_ERRNO(errno);
}

std::string Application::groupName() {
  if (struct group* gr = getgrgid(getgid()))
    return gr->gr_name;

  RAISE_ERRNO(errno);
}

#if !defined(HOST_NAME_MAX)
# define HOST_NAME_MAX 64
#endif

std::string Application::hostname() {
  char name[HOST_NAME_MAX];
  if (gethostname(name, sizeof(name)) == 0)
    return name;

  return "";
}

void Application::dropPrivileges(const std::string& username,
                                 const std::string& groupname) {
  if (username == Application::userName() && groupname == Application::groupName())
    return;

  logDebug("application", "dropping privileges to $0:$1", username, groupname);

  if (!groupname.empty() && !getgid()) {
    if (struct group* gr = getgrnam(groupname.c_str())) {
      if (setgid(gr->gr_gid) != 0) {
        logError("application", "could not setgid to $0: $1",
                 groupname, strerror(errno));
        return;
      }

      setgroups(0, nullptr);

      if (!username.empty()) {
        initgroups(username.c_str(), gr->gr_gid);
      }
    } else {
      logError("application", "Could not find group: $0", groupname);
      return;
    }
    logTrace("application", "Dropped group privileges to '$0'.", groupname);
  }

  if (!username.empty() && !getuid()) {
    if (struct passwd* pw = getpwnam(username.c_str())) {
      if (setuid(pw->pw_uid) != 0) {
        logError("application", "could not setgid to $0: $1",
                 username, strerror(errno));
        return;
      }
      logInfo("application", "Dropped privileges to user $0", username);

      if (chdir(pw->pw_dir) < 0) {
        logError("application", "could not chdir to $0: $1",
                 pw->pw_dir, strerror(errno));
        return;
      }
    } else {
      logError("application", "Could not find group: $0", groupname);
      return;
    }

    logTrace("application", "Dropped user privileges to '$0'.", username);
  }

  if (!::getuid() || !::geteuid() || !::getgid() || !::getegid()) {
#if defined(X0_RELEASE)
    logError("application",
             "Service is not allowed to run with administrative permissions. "
             "Service is still running with administrative permissions.");
#else
    logWarning("application",
               "Service is still running with administrative permissions.");
#endif
  }
}

void Application::daemonize() {
  // XXX raises a warning on OS/X, but heck, how do you do it then on OS/X?
  if (::daemon(true /*no chdir*/, true /*no close*/) < 0) {
    RAISE_ERRNO(errno);
  }
}

} // namespace xzero
