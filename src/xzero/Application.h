// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/Api.h>
#include <xzero/logging.h>

namespace xzero {

class Executor;

using ProcessID = int64_t;

class Application {
 public:
  static void init();

  static void logToStderr(LogLevel loglevel = LogLevel::Info);

  static void installGlobalExceptionHandler();

  /**
   * Retrieves the application name, as determined by inspecting the system
   * environment.
   */
  static std::string appName();

  /**
   * Retrieves the user-name this application is running under.
   */
  static std::string userName();

  /**
   * Retrieves the group-name this application is running under.
   */
  static std::string groupName();

  /**
   * Retrieves the underlying OS hostname.
   */
  static std::string hostname();

  /**
   * Drops privileges to given @p user and @p group.
   *
   * Will only actually perform the drop if currently running as root
   * and the respective values are non-empty.
   */
  static void dropPrivileges(const std::string& user, const std::string& group);

  /**
   * Forks the application into background and become a daemon.
   */
  static void daemonize();

  /**
   * Retrieves the system's page size in bytes.
   */
  static size_t pageSize();

  /**
   * Retrieves the number of available processors on the system system.
   */
  static size_t processorCount();

  static ProcessID processId();

  static bool isWSL();
};

} // namespace xzero
