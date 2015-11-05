// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/Application.h>
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

namespace xzero {

void Application::init() {
  Application::installGlobalExceptionHandler();

  thread::SignalHandler::ignore(SIGPIPE);

  // well, when you detach from the terminal, you're garanteed to not get one.
  // unless someone sends it explicitely (so why ignoring then)?
  thread::SignalHandler::ignore(SIGHUP);
}

void Application::logToStderr(LogLevel loglevel) {
  Logger::get()->setMinimumLogLevel(loglevel);
  Logger::get()->addTarget(ConsoleLogTarget::get());
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
