#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <initializer_list>
#include <vector>
#include <string>

// supervisor [-f|--fork] [-p|--pidfile=PATH] -- cmd ...
//
// -f,--fork            whether to fork the supervisor process into background
// -p,--pidfile=PATH    location to store the current supervisor PID
// -r,--restart
// -c,--cgroups         Use cgroups to track PIDs
//
// Examples:
//     supervisor /usr/sbin/x0d --no-fork
//     supervisor -p /var/run/xzero/supervisor.pid -- /usr/sbin/x0d --no-fork

static int childPid = 0;
static int lastSignum = 0;
static std::string programPath;
static std::vector<std::string> programArgs;
static int retryCount = 5;

bool isTerminateSignal(int sig) {
  return sig == SIGTERM || sig == SIGQUIT || sig == SIGINT || sig == SIGHUP;
}

void runProgram() {
  printf("Running program...\n");

  pid_t pid = fork();

  if (pid < 0) {
    fprintf(stderr, "fork failed. %s\n", strerror(errno));
    abort();
  } else if (pid > 0) {
    // parent
    childPid = pid;
    printf("supervisor: child pid is %d\n", pid);
  } else {
    // child

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

bool restartProgram() {
  if (retryCount == 0) {
    return false;
  }

  --retryCount;

  printf("Restarting program (retry count: %d)\n", retryCount);
  runProgram();
  return true;
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
  if (!parseArgs(argc, argv)) return 1;

  printf("Installing signal handler...\n");
  for (int sig : {SIGINT, SIGTERM, SIGQUIT, SIGHUP}) {
    signal(sig, &sighandler);
  }

  runProgram();

  while (childPid != 0) {
    int status = 0;
    if (waitpid(childPid, &status, 0) < 0) {
      perror("waitpid");
      break;
    }

    if (WIFEXITED(status)) {
      printf("Child %d terminated with exit code %d\n", childPid,
             WEXITSTATUS(status));
      restartProgram();
      // exit(WEXITSTATUS(status));
    }

    if (WIFSIGNALED(status)) {
      printf("Child %d terminated with signal %s (%d)\n", childPid,
             strsignal(WTERMSIG(status)), WTERMSIG(status));
      restartProgram();
      // exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Child %d terminated. Status code %d\n", childPid, status);
    restartProgram();
    // break;
  }

  return 0;
}

// vim:ts=2:sw=2
