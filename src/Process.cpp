// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/Process.h>
#include <string>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <cstdio>
#include <ctime>
#include <cerrno>

namespace base {

/** invokes \p cmd until its not early aborted with EINTR. */
#define EINTR_LOOP(cmd)                                          \
  {                                                              \
    int _rv;                                                     \
    do {                                                         \
      _rv = cmd;                                                 \
    } while (_rv == -1 && errno == EINTR);                       \
    if (_rv < 0) {                                               \
      fprintf(stderr, "EINTR_LOOP(%s): failed with: %s\n", #cmd, \
              strerror(errno));                                  \
    }                                                            \
  }

Process::Process(struct ev_loop* loop)
    : loop_(loop), input_(), output_(), error_(), pid_(-1), status_(0) {}

Process::Process(struct ev_loop* loop, const std::string& exe,
                 const ArgumentList& args, const Environment& env,
                 const std::string& workdir)
    : loop_(loop), input_(), output_(), error_(), pid_(-1), status_(0) {
  start(exe, args, env, workdir);
}

Process::~Process() {
  if (pid_ > 0) EINTR_LOOP(::waitpid(pid_, &status_, 0));

  // fprintf(stderr, "~Process(): rv=%d, errno=%s\n", rv, strerror(errno));
}

void Process::dumpCore() {
  int child = fork();

  switch (child) {
    case -1:  // fork error
      fprintf(stderr, "Process.dumpCore: fork error: %s\n", strerror(errno));
      break;
    case 0: {  // child
      abort();
      break;
    }
    default: {  // parent
      int status = 0;
      EINTR_LOOP(::waitpid(child, &status, 0));
      char filename[80];
      time_t now = time(nullptr);
      struct tm tm;
      localtime_r(&now, &tm);
      strftime(filename, sizeof(filename), "%Y%m%d-%H%M%S.core", &tm);
      int rv = ::rename("core", filename);
      if (rv < 0) {
        fprintf(stderr, "Process.dumpCore: core rename error: %s\n",
                strerror(errno));
      } else {
        fprintf(stderr, "Process.dumpCore: %s\n", filename);
      }
    }
  }
}

int Process::start(const std::string& exe, const ArgumentList& args,
                   const Environment& env, const std::string& workdir) {
#if !defined(XZERO_NDEBUG)
  //::fprintf(stderr, "proc[%d] start(exe=%s, args=[...], workdir=%s)\n",
  //getpid(), exe.c_str(), workdir.c_str());
  for (int i = 3; i < 32; ++i)
    if (!(fcntl(i, F_GETFD) & FD_CLOEXEC))
      fprintf(stderr, "Process: fd %d still open\n", i);
#endif

  switch (pid_ = vfork()) {
    case -1:  // error
      fprintf(stderr, "Process: error starting process: %s\n", strerror(errno));
      return -1;
    case 0:  // child
      setupChild(exe, args, env, workdir);
      break;
    default:  // parent
      setupParent();
      break;
  }
  return 0;
}

bool Process::terminate() {
  if (pid_ <= 0) {
    errno = EINVAL;
    return false;
  }

  if (::kill(pid_, SIGTERM) < 0) {
    return false;
  }

  return true;
}

bool Process::kill() {
  if (pid_ <= 0) {
    errno = EINVAL;
    return false;
  }

  if (::kill(pid_, SIGKILL) < 0) {
    return false;
  }

  return true;
}

void Process::setStatus(int status) {
  status_ = status;

  if (WIFEXITED(status_) || WIFSIGNALED(status_))
    // process terminated (normally or by signal).
    // so mark process as exited by resetting the PID
    pid_ = -1;
}

/** tests whether child Process has exited already.
 */
bool Process::expired() {
  if (pid_ <= 0) return true;

  int rv;
  EINTR_LOOP(rv = ::waitpid(pid_, &status_, WNOHANG));

  if (rv == 0)
    // child not exited yet
    return false;

  if (rv < 0)
    // error
    return false;

  pid_ = -1;
  return true;
}

void Process::setupParent() {
  // setup I/O
  input_.closeRemote();
  output_.closeRemote();
  error_.closeRemote();
}

void Process::setupChild(const std::string& _exe, const ArgumentList& _args,
                         const Environment& _env, const std::string& _workdir) {
  // restore signal handler(s)
  ::signal(SIGPIPE, SIG_DFL);

  // setup environment
  int k = 0;
  std::vector<char*> env(_env.size() + 1);

  for (Environment::const_iterator i = _env.cbegin(), e = _env.cend(); i != e;
       ++i) {
    char* buf = new char[i->first.size() + i->second.size() + 2];
    ::memcpy(buf, i->first.c_str(), i->first.size());
    buf[i->first.size()] = '=';
    ::memcpy(buf + i->first.size() + 1, i->second.c_str(),
             i->second.size() + 1);

    //::fprintf(stderr, "proc[%d]: setting env[%d]: %s\n", getpid(), k, buf);
    //::fflush(stderr);
    env[k++] = buf;
  }
  env[_env.size()] = 0;

  // setup args
  std::vector<char*> args(_args.size() + 2);
  args[0] = const_cast<char*>(_exe.c_str());
  //::fprintf(stderr, "args[%d] = %s\n", 0, args[0]);
  for (int i = 0, e = _args.size(); i != e; ++i) {
    args[i + 1] = const_cast<char*>(_args[i].c_str());
    //::fprintf(stderr, "args[%d] = %s\n", i + 1, args[i + 1]);
  }
  args[args.size() - 1] = 0;

  // chdir
  if (!_workdir.empty()) {
    ::chdir(_workdir.c_str());
  }

  // setup I/O
  EINTR_LOOP(::close(STDIN_FILENO));
  EINTR_LOOP(::close(STDOUT_FILENO));
  EINTR_LOOP(::close(STDERR_FILENO));

  EINTR_LOOP(::dup2(input_.remote(), STDIN_FILENO));
  EINTR_LOOP(::dup2(output_.remote(), STDOUT_FILENO));
  EINTR_LOOP(::dup2(error_.remote(), STDERR_FILENO));

#if 0  // this is basically working but a very bad idea for high performance
       // (XXX better get O_CLOEXEC working)
    for (int i = 3; i < 1024; ++i)
        ::close(i);
#endif

  //	input_.close();
  //	output_.close();
  //	error_.close();

  // finally execute
  ::execve(args[0], &args[0], &env[0]);

  // OOPS
  ::fprintf(stderr, "proc[%d]: execve(%s) error: %s\n", getpid(), args[0],
            strerror(errno));
  ::fflush(stderr);
  ::_exit(1);
}

}  // namespace base
