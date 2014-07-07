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
#include <fcntl.h>
#include <initializer_list>
#include <vector>
#include <fstream>
#include <iostream>
#include <string>

// supervisor [-f|--fork] [-p|--pidfile=PATH] -- cmd ...
//
// -f,--fork            whether to fork the supervisor process into background
// -p,--pidfile=PATH    location to store the current supervisor PID
// -r,--restart         Automatically restart program, if crashed.
// -c,--cgroups         Use cgroups to track PIDs
//
// Examples:
//     supervisor /usr/sbin/x0d --no-fork
//     supervisor -p /var/run/xzero/supervisor.pid -- /usr/sbin/x0d --no-fork

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
  snprintf(path, sizeof(path), "/sys/fs/cgroup/cpu/%d.supervisor/tasks", getpid());

  char buf[64];
  ssize_t n = snprintf(buf, sizeof(buf), "%d", pid);

  int fd = open(path, O_WRONLY);
  write(fd, buf, n);
  close(fd);
}

std::vector<int> PidTracker::get() {
  std::vector<int> result;

  char path[80];
  snprintf(path, sizeof(path), "/sys/fs/cgroup/cpu/%d.supervisor/tasks", getpid());

  std::ifstream tasksFile(path);
  std::string line;

  while (std::getline(tasksFile, line)) {
    result.push_back(stoi(line));
  }

  return result;
}

void PidTracker::dump() {
  printf("PID tracking: ");
  const auto pids = get();
  for (int pid: pids) {
    printf(" %d", pid);
  }
  printf("\n");
}

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
  } else if (pid > 0) { // parent
    childPid = pid;
    pidTracker.add(pid);
    printf("supervisor: child pid is %d\n", pid);
    pidTracker.dump();
  } else { // child
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

bool parseArgs(int argc, const char* argv[]) {
  if (argc <= 1) {
    fprintf(stderr, "usage error\n");
    return false;
  }

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
    for (int sig : {SIGINT, SIGTERM, SIGQUIT, SIGHUP}) {
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

      fprintf(stderr, "Child %d terminated. Status code %d\n", childPid, status);
      restartProgram();
    }
  } catch (int exitCode) {
    return exitCode;
  }
}

// vim:ts=2:sw=2
