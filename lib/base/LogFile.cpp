/* <x0/LogFile.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/LogFile.h>
#include <x0/Buffer.h>
#include <x0/io/SinkVisitor.h>
#include <x0/Api.h>
#include <x0/sysconfig.h>

#include <memory>
#include <atomic>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <ev++.h>

namespace x0 {

LogFile::LogFile(const std::string& path) :
    path_(path),
    fileFd_(-1),
    senderFd_(-1),
    receiverFd_(-1),
    pending_(0),
    dropped_(0),
    writeErrors_(0),
    readBuffer_(1024 * sizeof(void*)),
    tid_(),
    loop_(ev_loop_new(0)),
    onData_(loop_),
    onCycle_(loop_),
    onStop_(loop_)
{
    init();
}

LogFile::~LogFile()
{
    onStop_.send();

    if (receiverFd_ != -1) {
        pthread_join(tid_, nullptr);
    }

    // consume read remaining data
    while (readSome()) {
        onData(onData_, ev::READ);
    }

    ::close(fileFd_);
    ::close(senderFd_);
    ::close(receiverFd_);

    ev_loop_destroy(loop_);
}

void LogFile::init()
{
    int flags = 0;

#if defined(SOCK_CLOEXEC)
    flags |= SOCK_CLOEXEC;
#endif

#if defined(SOCK_NONBLOCK)
    flags |= SOCK_NONBLOCK;
#endif

    int protocol = 0;
    int sockets[2];
    int rv = socketpair(AF_UNIX, SOCK_STREAM | flags, protocol, sockets);

    if (rv == 0) {
        senderFd_ = sockets[0];
        receiverFd_ = sockets[1];

#if !defined(SOCK_NONBLOCK)
        fcntl(senderFd_, F_SETFD, fcntl(senderFd_, F_GETFL) | O_NONBLOCK);
        fcntl(receiverFd_, F_SETFD, fcntl(senderFd_, F_GETFL) | O_NONBLOCK);
#endif

#if !defined(SOCK_CLOEXEC)
        fcntl(senderFd_, F_SETFD, fcntl(senderFd_, F_GETFL) | FD_CLOEXEC);
        fcntl(receiverFd_, F_SETFD, fcntl(receiverFd_, F_GETFL) | FD_CLOEXEC);
#endif

        shutdown(senderFd_, SHUT_RD);
        shutdown(receiverFd_, SHUT_WR);

        fileFd_ = open();

        onData_.set<LogFile, &LogFile::onData>(this);
        onData_.set(receiverFd_, ev::READ);
        onData_.start();

        onStop_.set<LogFile, &LogFile::onStop>(this);
        onStop_.start();
        loop_.unref();

        onCycle_.set<LogFile, &LogFile::onCycle>(this);
        onCycle_.start();
        loop_.unref();

        pthread_create(&tid_, nullptr, &LogFile::start, this);
    }
}

int LogFile::open()
{
    int flags = O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC;

#if defined(O_LARGEFILE)
    flags |= O_LARGEFILE;
#endif

#if defined(O_NOATIME)
    flags |= O_NOATIME;
#endif

    mode_t mode = 0644;
    int fd = ::open(path_.c_str(), flags, mode);

    return fd;
}

void LogFile::cycle()
{
    onCycle_.send();
}

void LogFile::onStop(ev::async& async, int revents)
{
    loop_.ref();
    onCycle_.stop();

    loop_.ref();
    onStop_.stop();

    onData_.stop();
}

void LogFile::onCycle(ev::async& async, int revents)
{
    int newFd = open();
    if (newFd < 0) {
        std::unique_ptr<Buffer> msg(new Buffer);
        msg->printf("Could not re-open logfile %s. %s\n", path_.c_str(), strerror(errno));
        write(std::move(msg));
    } else {
        int oldFd = fileFd_;
        fileFd_ = newFd;
        ::close(oldFd);
    }
}

bool LogFile::write(std::unique_ptr<Buffer>&& message)
{
    Buffer* buf = message.get();

    if (::write(senderFd_, &buf, sizeof(void*)) != sizeof(void*)) {
        dropped_++;
        return false;
    }

    message.release(); // transfer message ownership to write-thread

    return true;
}

ssize_t LogFile::write(const void *buffer, size_t size)
{
    std::unique_ptr<Buffer> msg(new Buffer(size + 1));
    msg->push_back(buffer, size);
    write(std::move(msg));
    return size;
}

void LogFile::accept(SinkVisitor& v)
{
    v.visit(*this);
}

void* LogFile::start(void* self)
{
    reinterpret_cast<LogFile*>(self)->main();
    return nullptr;
}

void LogFile::main()
{
#if defined(HAVE_PTHREAD_SETNAME_NP)
    pthread_setname_np(tid_, "xzero-logger");
#endif

    loop_.run();
}

void LogFile::onData(ev::io& io, int revents)
{
    size_t n = readSome();
    if (n == 0) {
        return;
    }

    iovec* iov = new iovec[n];
    size_t vcount = 0;

    for (size_t i = 0; i != n; ++i) {
        Buffer* msg = reinterpret_cast<Buffer**>(readBuffer_.data())[i];
        assert(msg != nullptr);

        iov[vcount].iov_base = msg->data();
        iov[vcount].iov_len = msg->size();

        ++vcount;
    }

    // move possible valid tail data to the beginning of the read-buffer, and cut the rest
    readBuffer_ = readBuffer_.ref(n * sizeof(void*));

    ssize_t rv = ::writev(fileFd_, iov, vcount);
    if (rv < 0) {
        writeErrors_++;
    }

    for (size_t i = 0; i != vcount; ++i) {
        delete reinterpret_cast<Buffer**>(readBuffer_.data())[i];
    }

    delete[] iov;
}

/**
 * reads as some log messages from the transfer buffer.
 *
 * @return number of fully read message pointers.
 */
size_t LogFile::readSome()
{
    // ensure enough free space
    if (readBuffer_.capacity() - readBuffer_.size() < 8 * sizeof(void*)) {
        readBuffer_.reserve(readBuffer_.capacity() * 2);
    }

retry:
    ssize_t rv = ::read(receiverFd_, readBuffer_.end(), readBuffer_.capacity() - readBuffer_.size());

    if (rv < 0) {
        switch (errno) {
            case EINTR:
                goto retry;
            case EAGAIN:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
            case EWOULDBLOCK:
#endif
            default:
                return readBuffer_.size() / sizeof(void*);
        }
    } else {
        readBuffer_.resize(readBuffer_.size() + rv);
    }

    return readBuffer_.size() / sizeof(void*);
}

} // namespace x0
