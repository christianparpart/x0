/* <XzeroEventHandler.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0d/XzeroEventHandler.h>
#include <x0d/XzeroDaemon.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/flow/FlowRunner.h>
#include <x0/io/SyslogSink.h>
#include <x0/Tokenizer.h>
#include <x0/Logger.h>
#include <x0/DateTime.h>
#include <x0/strutils.h>
#include <x0/Severity.h>
#include <x0/DebugLogger.h>
#include <x0/sysconfig.h>

#include <ev++.h>
#include <sd-daemon.h>

#include <functional>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdarg>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>

#define SIG_X0_SUSPEND (SIGRTMIN+4)
#define SIG_X0_RESUME (SIGRTMIN+5)

// {{{ helper
static std::string sig2str(int sig)
{
	static const char* sval[_NSIG + 1] = {
		nullptr,		// 0
		"SIGHUP",		// 1
		"SIGINT",		// 2
		"SIGQUIT",		// 3
		"SIGILL",		// 4
		"SIGTRAP",		// 5
		"SIGIOT",		// 6
		"SIGBUS",		// 7
		"SIGFPE",		// 8
		"SIGKILL",		// 9

		"SIGUSR1",		// 10
		"SIGSEGV",		// 11
		"SIGUSR2",		// 12
		"SIGPIPE",		// 13
		"SIGALRM",		// 14
		"SIGTERM",		// 15
		"SIGSTKFLT",	// 16
		"SIGCHLD",		// 17
		"SIGCONT",		// 18
		"SIGSTOP",		// 19

		"SIGTSTP",		// 20
		"SIGTTIN",		// 21
		"SIGTTOU",		// 22
		"SIGURG	",		// 23
		"SIGXCPU",		// 24
		"SIGXFSZ",		// 25
		"SIGVTALRM",	// 26
		"SIGPROF",		// 27
		"SIGWINCH",		// 28
		"SIGIO",		// 29

		"SIGPWR",		// 30
		"SIGSYS",		// 31
		nullptr
	};

	char buf[64];
	if (sig >= SIGRTMIN && sig <= SIGRTMAX) {
		snprintf(buf, sizeof(buf), "SIGRTMIN+%d (%d)", sig - SIGRTMIN, sig);
	} else {
		snprintf(buf, sizeof(buf), "%s (%d)", sval[sig], sig);
	}
	return buf;
}
// }}}

namespace x0d {

using namespace x0;

XzeroEventHandler::XzeroEventHandler(XzeroDaemon* daemon, ev::loop_ref loop) :
	daemon_(daemon),
	loop_(loop),
	state_(XzeroState::Inactive),
	terminateSignal_(loop_),
	ctrlcSignal_(loop_),
	quitSignal_(loop_),
	user1Signal_(loop_),
	hupSignal_(loop_),
	terminationTimeout_(loop_),
	child_(loop_)
{
	setState(XzeroState::Initializing);

	terminateSignal_.set<XzeroEventHandler, &XzeroEventHandler::quickShutdownHandler>(this);
	terminateSignal_.start(SIGTERM);
	ev_unref(loop_);

	ctrlcSignal_.set<XzeroEventHandler, &XzeroEventHandler::quickShutdownHandler>(this);
	ctrlcSignal_.start(SIGINT);
	ev_unref(loop_);

	quitSignal_.set<XzeroEventHandler, &XzeroEventHandler::gracefulShutdownHandler>(this);
	quitSignal_.start(SIGQUIT);
	ev_unref(loop_);

	user1Signal_.set<XzeroEventHandler, &XzeroEventHandler::reopenLogsHandler>(this);
	user1Signal_.start(SIGUSR1);
	ev_unref(loop_);

	hupSignal_.set<XzeroEventHandler, &XzeroEventHandler::reexecHandler>(this);
	hupSignal_.start(SIGHUP);
	ev_unref(loop_);

	suspendSignal_.set<XzeroEventHandler, &XzeroEventHandler::suspendHandler>(this);
	suspendSignal_.start(SIG_X0_SUSPEND);
	ev_unref(loop_);

	resumeSignal_.set<XzeroEventHandler, &XzeroEventHandler::resumeHandler>(this);
	resumeSignal_.start(SIG_X0_RESUME);
	ev_unref(loop_);
}

XzeroEventHandler::~XzeroEventHandler()
{
	if (terminationTimeout_.is_active()) {
		ev_ref(loop_);
		terminationTimeout_.stop();
	}

	if (terminateSignal_.is_active()) {
		ev_ref(loop_);
		terminateSignal_.stop();
	}

	if (ctrlcSignal_.is_active()) {
		ev_ref(loop_);
		ctrlcSignal_.stop();
	}

	if (quitSignal_.is_active()) {
		ev_ref(loop_);
		quitSignal_.stop();
	}

	if (user1Signal_.is_active()) {
		ev_ref(loop_);
		user1Signal_.stop();
	}

	if (hupSignal_.is_active()) {
		ev_ref(loop_);
		hupSignal_.stop();
	}

	if (suspendSignal_.is_active()) {
		ev_ref(loop_);
		suspendSignal_.stop();
	}

	if (resumeSignal_.is_active()) {
		ev_ref(loop_);
		resumeSignal_.stop();
	}
}

x0::HttpServer* XzeroEventHandler::server() const
{
	return daemon_->server();
}

/*! Updates state change, and tell supervisors about our state change.
 */
void XzeroEventHandler::setState(XzeroState newState)
{
	if (state_ == newState) {
		// most probabely a bug
	}

	switch (newState) {
		case XzeroState::Inactive:
			break;
		case XzeroState::Initializing:
			sd_notify(0, "STATUS=Initializing ...");
			break;
		case XzeroState::Running:
			if (server()->generation() == 1) {
				// we have been started up directoy (e.g. by systemd)
				sd_notifyf(0,
					"MAINPID=%d\n"
					"STATUS=Accepting requests ...\n"
					"READY=1\n",
					getpid()
				);
			} else {
				// we have been invoked by x0d itself, e.g. a executable upgrade and/or
				// configuration reload.
				// Tell the parent-x0d to shutdown gracefully.
				// On receive, the parent process will tell systemd, that we are the new master.
				::kill(getppid(), SIGQUIT);
			}
			break;
		case XzeroState::Upgrading:
			sd_notify(0, "STATUS=Upgrading");
			server()->log(x0::Severity::info, "Upgrading ...");
			break;
		case XzeroState::GracefullyShuttingdown:
			if (state_ == XzeroState::Running) {
				sd_notify(0, "STATUS=Shutting down gracefully ...");
			} else if (state_ == XzeroState::Upgrading) {
				// we're not the master anymore
				// tell systemd, that our freshly spawned child is taking over, and the new master
				// XXX as of systemd v28, RELOADED=1 is not yet implemented, but on their TODO list
				sd_notifyf(0,
					"MAINPID=%d\n"
					"STATUS=Accepting requests ...\n"
					"RELOADED=1\n",
					child_.pid
				);
			}
			break;
		default:
			// must be a bug
			break;
	}

	state_ = newState;
}

void XzeroEventHandler::reexecHandler(ev::sig& sig, int)
{
	daemon_->reexec();
}

void XzeroEventHandler::reopenLogsHandler(ev::sig&, int)
{
	server()->log(x0::Severity::info, "Reopening of all log files requested.");
	daemon_->cycleLogs();
}

/** temporarily suspends processing new and currently active connections.
 */
void XzeroEventHandler::suspendHandler(ev::sig& sig, int)
{
	// suspend worker threads while performing the reexec
	for (x0::HttpWorker* worker: server()->workers()) {
		worker->suspend();
	}

	for (x0::ServerSocket* listener: server()->listeners()) {
		// stop accepting new connections
		listener->stop();
	}
}

/** resumes previousely suspended execution.
 */
void XzeroEventHandler::resumeHandler(ev::sig& sig, int)
{
	server()->log(x0::Severity::debug, "Siganl %s received.", sig2str(sig.signum).c_str());

	server()->log(x0::Severity::debug, "Resuming worker threads.");
	for (x0::HttpWorker* worker: server()->workers()) {
		worker->resume();
	}
}

// stage-1 termination handler
void XzeroEventHandler::gracefulShutdownHandler(ev::sig& sig, int)
{
	server()->log(x0::Severity::info, "%s received. Shutting down gracefully.", sig2str(sig.signum).c_str());

	for (x0::ServerSocket* listener: server()->listeners())
		listener->close();

	if (state_ == XzeroState::Upgrading) {
		child_.stop();

		for (x0::HttpWorker* worker: server()->workers()) {
			worker->resume();
		}
	}
	setState(XzeroState::GracefullyShuttingdown);

	// initiate graceful server-stop
	server()->maxKeepAlive = x0::TimeSpan::Zero;
	server()->stop();
}

// stage-2 termination handler
void XzeroEventHandler::quickShutdownHandler(ev::sig& sig, int)
{
	daemon_->log(x0::Severity::info, "%s received. shutting down NOW.", sig2str(sig.signum).c_str());

	if (state_ != XzeroState::Upgrading) {
		// we are no garbage parent process
		sd_notify(0, "STATUS=Shutting down.");
	}

	// default to standard signal-handler
	ev_ref(loop_);
	sig.stop();

	// install shutdown timeout handler
	terminationTimeout_.set<XzeroEventHandler, &XzeroEventHandler::quickShutdownTimeout>(this);
	terminationTimeout_.start(10, 0);
	ev_unref(loop_);

	// kill active HTTP connections
	server()->kill();
}

void XzeroEventHandler::quickShutdownTimeout(ev::timer&, int)
{
	daemon_->log(x0::Severity::warn, "Quick shutdown timed out. Terminating.");

	ev_ref(loop_);
	terminationTimeout_.stop();

	ev_break(loop_, ev::ALL);
}

/**
 * Watches over x0d-fork.
 * \param pid Child process ID.
 *
 */
void XzeroEventHandler::setupChild(int pid)
{
	child_.set<XzeroEventHandler, &XzeroEventHandler::onChild>(this);
	child_.set(pid, 0);
	child_.start();
}

void XzeroEventHandler::onChild(ev::child&, int)
{
	// the child exited before we receive a SUCCESS from it. so resume normal operation again.
	server()->log(x0::Severity::error, "New process exited with %d. Resuming normal operation.");

	child_.stop();

	// reenable HUP-signal
	if (!hupSignal_.is_active()) {
		server()->log(x0::Severity::error, "Reenable HUP-signal.");
		hupSignal_.start();
		ev_unref(loop_);
	}

	server()->log(x0::Severity::debug, "Reactivating listeners.");
	for (x0::ServerSocket* listener: server()->listeners()) {
		// reenable O_CLOEXEC on listener socket
		listener->setCloseOnExec(true);

		// start accepting new connections
		listener->start();
	}

	server()->log(x0::Severity::debug, "Resuming workers.");
	for (x0::HttpWorker* worker: server()->workers()) {
		worker->resume();
	}
}

} // namespace x0d
