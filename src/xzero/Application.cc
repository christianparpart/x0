// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/defines.h>
#include <xzero/sysconfig.h>

#if defined(XZERO_OS_WIN32)
#include <WinSock2.h>
#include <Windows.h>
#include <lmcons.h>
#include <process.h>
#else
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <sys/utsname.h>
#endif

#include <xzero/Application.h>
#include <xzero/Buffer.h>
#include <xzero/StringUtil.h>
#include <xzero/io/FileUtil.h>
#include <xzero/executor/Executor.h>
#include <xzero/thread/SignalHandler.h>
#include <xzero/logging.h>
#include <xzero/RuntimeError.h>
#include <xzero/sysconfig.h>
#include <fmt/format.h>
#include <cstdlib>
#include <fcntl.h>

namespace xzero {

void Application::init() {
  Application::installGlobalExceptionHandler();

#if !defined(XZERO_OS_WIN32)
  thread::SignalHandler::ignore(SIGPIPE);

  // well, when you detach from the terminal, you're garanteed to not get one.
  // unless someone sends it explicitely (so why ignoring then)?
  thread::SignalHandler::ignore(SIGHUP);
#endif
}

std::string Application::appName() {
#if defined(XZERO_OS_WIN32)
  TCHAR fileName[MAX_PATH + 1];
  GetModuleFileName(NULL, fileName, sizeof(fileName));
  return fileName;
#else
  return StringUtil::split(
      FileUtil::read(fmt::format("/proc/{}/cmdline", getpid())).str(),
      " ")[0];
#endif
}

void Application::logToStderr(LogLevel loglevel) {
  Logger::get()->setMinimumLogLevel(loglevel);
  Logger::get()->addTarget(ConsoleLogTarget::get());
}

static void globalEH() {
  try {
    throw;
  } catch (const std::exception& e) {
    logFatal("Unhandled exception caught. {}", e.what());
  } catch (...) {
    // d'oh
    fprintf(stderr, "Unhandled foreign exception caught.\n");
    abort();
  }
}

void Application::installGlobalExceptionHandler() {
  std::set_terminate(&globalEH);
}

std::string Application::userName() {
#if defined(XZERO_OS_WIN32)
  char username[UNLEN + 1];
  DWORD slen = UNLEN + 1;
  if (GetUserName(username, &slen))
    return std::string(username, slen);
  else
    return std::string();
#else
  if (struct passwd* pw = getpwuid(getuid()))
    return pw->pw_name;

  RAISE_ERRNO(errno);
#endif
}

std::string Application::groupName() {
#if defined(XZERO_OS_WIN32)
  return "";
#else
  if (struct group* gr = getgrgid(getgid()))
    return gr->gr_name;

  RAISE_ERRNO(errno);
#endif
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
#if defined(XZERO_OS_WIN32)
  // TODO: is that even possible?
#else
  if (username == Application::userName() && groupname == Application::groupName())
    return;

  logDebug("Dropping privileges to {}:{}", username, groupname);

  if (!groupname.empty() && !getgid()) {
    if (struct group* gr = getgrnam(groupname.c_str())) {
      if (setgid(gr->gr_gid) != 0) {
        logError("Could not setgid to {}: {}", groupname, strerror(errno));
        return;
      }

      setgroups(0, nullptr);

      if (!username.empty()) {
        initgroups(username.c_str(), gr->gr_gid);
      }
    } else {
      logError("Could not find group: {}", groupname);
      return;
    }
    logTrace("Dropped group privileges to '{}'.", groupname);
  }

  if (!username.empty() && !getuid()) {
    if (struct passwd* pw = getpwnam(username.c_str())) {
      if (setuid(pw->pw_uid) != 0) {
        logError("Could not setgid to {}: {}", username, strerror(errno));
        return;
      }
      logInfo("Dropped privileges to user {}", username);

      if (chdir(pw->pw_dir) < 0) {
        logError("Could not chdir to {}: {}", pw->pw_dir, strerror(errno));
        return;
      }
    } else {
      logError("Could not find group: {}", groupname);
      return;
    }

    logTrace("Dropped user privileges to '{}'.", username);
  }

  if (!::getuid() || !::geteuid() || !::getgid() || !::getegid()) {
#if defined(X0_RELEASE)
    logError("Service is not allowed to run with administrative permissions. "
             "Service is still running with administrative permissions.");
#else
    logWarning("Service is still running with administrative permissions.");
#endif
  }
#endif
}

void Application::daemonize() {
#if defined(XZERO_OS_WIN32)
  // TODO: how to become a service on Windows?
#else
  // XXX raises a warning on OS/X, but heck, how do you do it then on OS/X?
  if (::daemon(true /*no chdir*/, true /*no close*/) < 0) {
    RAISE_ERRNO(errno);
  }
#endif
}

size_t Application::pageSize() {
#if defined(HAVE_SYSCONF) && defined(_SC_PAGESIZE)
  int rv = sysconf(_SC_PAGESIZE);
  if (rv < 0)
    throw std::system_error(errno, std::system_category());

  return rv;
#else
  return 4096;
#endif
}

size_t Application::processorCount() {
#if defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
  int rv = sysconf(_SC_NPROCESSORS_ONLN);
  if (rv < 0)
    throw std::system_error(errno, std::system_category());

  return rv;
#else
  return 1;
#endif
}

ProcessID Application::processId() {
#if defined(XZERO_OS_WIN32)
  return GetCurrentProcessId();
#elif defined(HAVE_GETPID)
  return getpid();
#else
  return 0;
#endif
}

bool Application::isWSL() {
#if defined(XZERO_OS_LINUX)
  struct utsname uts;
  int rv = uname(&uts);
  if (rv < 0)
    RAISE_ERRNO(errno);

  return StringUtil::endsWith(uts.release, "Microsoft");
#else
  return false;
#endif
}

} // namespace xzero
