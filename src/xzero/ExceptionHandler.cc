// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <xzero/RuntimeError.h>
#include <xzero/ExceptionHandler.h>
#include <xzero/inspect.h>
#include <xzero/logging.h>
#include <xzero/StackTrace.h>

namespace xzero {

CatchAndLogExceptionHandler::CatchAndLogExceptionHandler(
    const std::string& component) :
    component_(component) {
}

void CatchAndLogExceptionHandler::onException(const std::exception& e) const {
  if (!component_.empty()) {
    logError("[{}] Unhandled exception caught. {}", component_, e.what());
  } else {
    logError("Unhandled exception caught. {}", e.what());
  }
}

CatchAndAbortExceptionHandler::CatchAndAbortExceptionHandler(
    const std::string& message) :
    message_(message) {}

void CatchAndAbortExceptionHandler::onException(
    const std::exception& error) const {
  fprintf(stderr, "%s\n\n", message_.c_str()); // FIXPAUL

  try {
    auto rte = dynamic_cast<const RuntimeError&>(error);
    //rte.debugPrint();
  } catch (const std::exception& cast_error) {
    fprintf(stderr, "foreign exception: %s\n", error.what());
  }

  fprintf(stderr, "Aborting...\n");
  abort(); // core dump if enabled
}

static std::string globalEHandlerMessage;

#if defined(XZERO_OS_WIN32)
static void globalSEGVHandler(int signo) {
  fprintf(stderr, "%s\n", globalEHandlerMessage.c_str());
  static const char *signame[] = { "INVALID", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE", "SIGKILL", "SIGUSR1", "SIGSEGV", "SIGUSR2", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT", "SIGCHLD", "SIGCONT", "SIGSTOP", "SIGTSTP", "SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU", "SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH", "SIGPOLL", "SIGPWR", "SIGSYS" };
  fprintf(stderr, "signal: %s\n", signame[signo]);
  //StackTrace strace;
  //strace.debugPrint(2);
  exit(EXIT_FAILURE);
}
#else
static void globalSEGVHandler(int sig, siginfo_t* siginfo, void* ctx) {
  fprintf(stderr, "%s\n", globalEHandlerMessage.c_str());
  fprintf(stderr, "signal: %s\n", strsignal(sig));
  //StackTrace strace;
  //strace.debugPrint(2);
  exit(EXIT_FAILURE);
}
#endif

static void globalEHandler() {
  fprintf(stderr, "%s\n", globalEHandlerMessage.c_str());

  auto ex = std::current_exception();
  if (ex == nullptr) {
    fprintf(stderr, "<no active exception>\n");
    return;
  }

  try {
    std::rethrow_exception(ex);
  } catch (const xzero::RuntimeError& e) {
    e.debugPrint();
  } catch (const std::exception& e) {
    fprintf(stderr, "foreign exception: %s\n", e.what());
  }

  exit(EXIT_FAILURE);
}

void CatchAndAbortExceptionHandler::installGlobalHandlers() {
  globalEHandlerMessage = message_;
  std::set_terminate(&globalEHandler);

  static const std::vector<int> sigs = {
    SIGSEGV,
    SIGABRT,
#if defined(SIGBUS)
    SIGBUS,
#endif
#if defined(SIGSYS)
    SIGSYS,
#endif
    SIGILL,
    SIGFPE
  };

#if defined(XZERO_OS_WIN32)
  for (int signo : sigs) {
    signal(signo, &globalSEGVHandler);
  }
#else
  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_sigaction = &globalSEGVHandler;
  sigact.sa_flags = SA_SIGINFO;

  for (int signo : sigs) {
    if (sigaction(signo, &sigact, nullptr) < 0) {
      RAISE_ERRNO(errno);
    }
  }
#endif
}

} // namespace xzero

