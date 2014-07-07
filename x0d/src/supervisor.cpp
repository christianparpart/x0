// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <getopt.h>
#include <initializer_list>
#include <vector>
#include <fstream>
#include <iostream>
#include <string>

// {{{ PidTracker
class PidTracker {
 public:
  PidTracker();
  ~PidTracker();

  void add(int pid);
  std::vector<int> get();
  void dump();
};

PidTracker::PidTracker() {
  char path[80];
  snprintf(path, sizeof(path), "/sys/fs/cgroup/cpu/%d.supervisor", getpid());
  int rv = mkdir(path, 0777);

  if (rv < 0) {
    perror("PidTracker: mkdir");
  }
}

PidTracker::~PidTracker() {
  char path[80];
  snprintf(path, sizeof(path), "/sys/fs/cgroup/cpu/%d.supervisor", getpid());
  rmdir(path);
}

void PidTracker::add(int pid) {
  char path[80];
  snprintf(path, sizeof(path), "/sys/fs/cgroup/cpu/%d.supervisor/tasks",
           getpid());

  char buf[64];
  ssize_t n = snprintf(buf, sizeof(buf), "%d", pid);

  int fd = open(path, O_WRONLY);
  write(fd, buf, n);
  close(fd);
}

std::vector<int> PidTracker::get() {
  std::vector<int> result;

  char path[80];
  snprintf(path, sizeof(path), "/sys/fs/cgroup/cpu/%d.supervisor/tasks",
           getpid());

  std::ifstream tasksFile(path);
  std::string line;

  while (std::getline(tasksFile, line)) {
    result.push_back(stoi(line));
  }

  return result;
}

void PidTracker::dump() {
  printf("PID tracking dump: ");
  const auto pids = get();
  for (int pid : pids) {
    printf(" %d", pid);
  }
  printf("\n");
}
// }}}

static std::vector<int> forwardingSignals;
static PidTracker pidTracker;
static int childPid = 0;
static int lastSignum = 0;
static std::string programPath;
static std::vector<std::string> programArgs;
static int retryCount = 5;

void runProgram() {
  printf("Running program...\n");

  pid_t pid = fork();

  if (pid < 0) {
    fprintf(stderr, "fork failed. %s\n", strerror(errno));
    abort();
  } else if (pid > 0) {  // parent
    childPid = pid;
    pidTracker.add(pid);
    printf("supervisor: child pid is %d\n", pid);
    pidTracker.dump();

    if (prctl(PR_SET_CHILD_SUBREAPER, 1) < 0) {
      fprintf(stderr, "supervisor: prctl(PR_SET_CHILD_SUBREAPER) failed. %s",
              strerror(errno));

      // if this one fails, we can still be functional to *SOME* degree,
      // like, auto-restarting still works, but
      // the supervised child is forking to re-exec, that'll not work then.
    }
  } else {  // child
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(programPath.c_str()));
    for (const std::string& arg : programArgs) {
      argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    execvp(programPath.c_str(), argv.data());
    fprintf(stderr, "execvp failed. %s\n", strerror(errno));
    abort();
  }
}

void restartProgram() {
  pidTracker.dump();
  const auto pids = pidTracker.get();
  if (pids.size() == 1) {
    childPid = pids[0];
    printf("supervisor: reattaching to child PID %d\n", childPid);
    return;
  }

  printf("Restarting program (retry count: %d)\n", retryCount);

  if (retryCount == 0) {
    throw EXIT_FAILURE;
  }

  retryCount--;
  runProgram();
}

void sighandler(int signum) {
  lastSignum = signum;

  if (childPid) {
    printf("Signal %s (%d) received. Forwarding to child PID %d.\n",
           strsignal(signum), signum, childPid);

    // forward to child process
    kill(childPid, signum);
  }
}

void printHelp() {
  printf(
      "supervisor: a process supervising tool\n"
      "  (c) 2009-2014 Christian Parpart <trapni@gmail.com>\n"
      "\n"
      "usage:\n"
      "  supervisor [-f|--fork] [-p|--pidfile=PATH] -- cmd ...\n"
      "\n"
      "options:\n"
      "  -f,--fork          whether to fork and daemonize the supervisor\n"
      "                     process into background\n"
      "  -p,--pidfile=PATH  location to store the current supervisor PID\n"
      "  -r,--restart       Automatically restart program, if crashed.\n"
      "  -d,--delay=SECONDS Number of seconds to wait before we retry\n"
      "                     to restart the application.\n"
      "  -s,--signal=SIGNAL Adds given signal to the list of signals\n"
      "                     to forward to the supervised program.\n"
      "                     Defaults to (INT, TERM, QUIT, USR1, USR2, HUP)\n"
      "  -P,--child-pidfile=PATH\n"
      "                     Path to the child process' managed PID file.\n"
      "                     The supervisor is watching this file for updates.\n"
      "\n"
      "Examples:\n"
      "    supervisor -- /usr/sbin/x0d --no-fork\n"
      "    supervisor -p /var/run/xzero/supervisor.pid -- /usr/sbin/x0d\\\n"
      "               --no-fork\n"
      "\n");
}

bool parseArgs(int argc, const char* argv[]) {
  if (argc <= 1) {
    printHelp();
    return false;
  }
  // TODO: use getopt_long

  int i = 1;

  programPath = argv[i];
  i++;

  while (argv[i] != nullptr) {
    programArgs.push_back(argv[i]);
    i++;
  }

  return true;
}

int main(int argc, const char* argv[]) {
  try {
    if (!parseArgs(argc, argv)) return 1;

    printf("Installing signal handler...\n");
    for (int sig : {SIGINT, SIGTERM, SIGQUIT, SIGHUP, SIGUSR1, SIGUSR2}) {
      signal(sig, &sighandler);
    }

    // if (setsid() < 0) {
    //   fprintf(stderr, "Error creating session. %s\n", strerror(errno));
    //   return EXIT_FAILURE;
    // }

    if (setpgrp() < 0) {
      fprintf(stderr, "Error creating process group. %s\n", strerror(errno));
      return EXIT_FAILURE;
    }

    runProgram();

    for (;;) {
      int status = 0;
      if (waitpid(childPid, &status, 0) < 0) {
        perror("waitpid");
        throw EXIT_FAILURE;
      }

      if (WIFEXITED(status)) {
        printf("Child %d terminated with exit code %d\n", childPid,
               WEXITSTATUS(status));
        restartProgram();
      }

      if (WIFSIGNALED(status)) {
        printf("Child %d terminated with signal %s (%d)\n", childPid,
               strsignal(WTERMSIG(status)), WTERMSIG(status));
        restartProgram();
      }

      fprintf(stderr, "Child %d terminated. Status code %d\n", childPid,
              status);
      restartProgram();
    }
  }
  catch (int exitCode) {
    return exitCode;
  }
}

// vim:ts=2:sw=2
