/* <x0/LogFile.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/LogFile.h>
#include <x0/Buffer.h>
#include <x0/Api.h>
#include <x0/sysconfig.h>

#include <memory>
#include <atomic>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>

namespace x0 {

LogFile::LogFile(const std::string& path) :
    path_(path),
    fileFd_(-1),
    senderFd_(-1),
    receiverFd_(-1),
    pending_(0),
    dropped_(0),
    writeErrors_(0),
    tid_()
{
    init();
}

LogFile::~LogFile()
{
    if (receiverFd_ != -1) {
        pthread_cancel(tid_);
        pthread_join(tid_, nullptr);
    }

    ::close(fileFd_);
    ::close(senderFd_);
    ::close(receiverFd_);
}

void LogFile::init()
{
    int flags = 0;

#if defined(SOCK_CLOEXEC)
    flags |= SOCK_CLOEXEC;
#endif

    int protocol = 0;
    int sockets[2];
    int rv = socketpair(AF_UNIX, SOCK_STREAM | flags, protocol, sockets);

    if (rv == 0) {
        senderFd_ = sockets[0];
        receiverFd_ = sockets[1];

        fcntl(senderFd_, F_SETFD, fcntl(senderFd_, F_GETFL) | O_NONBLOCK);

#if !defined(SOCK_CLOEXEC)
        fcntl(senderFd_, F_SETFD, fcntl(senderFd_, F_GETFL) | FD_CLOEXEC);
        fcntl(receiverFd_, F_SETFD, fcntl(receiverFd_, F_GETFL) | FD_CLOEXEC);
#endif

        shutdown(senderFd_, SHUT_RD);
        shutdown(receiverFd_, SHUT_WR);

        fileFd_ = open();

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
    void* ptr = nullptr;
    if (::write(senderFd_, &ptr, sizeof(ptr)) == sizeof(ptr)) {
        printf("triggered async cycle\n");
        return;
    }

    cycleNow();
}

bool LogFile::cycleNow()
{
    printf("cycleNow\n");
    int newFd = open();
    if (newFd < 0) {
        std::unique_ptr<Buffer> msg(new Buffer);
        msg->printf("Could not re-open logfile %s. %s\n", path_.c_str(), strerror(errno));
        write(std::move(msg));
        return false;
    } else {
        int oldFd = fileFd_;
        fileFd_ = newFd;
        ::close(oldFd);
        return true;
    }
}

bool LogFile::write(std::unique_ptr<Buffer>&& message)
{
    void* ptr = message.get();
    if (::write(senderFd_, &ptr, sizeof(ptr)) != sizeof(ptr)) {
        dropped_++;
        return false;
    }

    message.release();

    return true;
}

void* LogFile::start(void* self)
{
    reinterpret_cast<LogFile*>(self)->main();
    return nullptr;
}

void LogFile::main()
{
#if defined(HAVE_PTHREAD_SETNAME_NP)
    pthread_setname_np(tid_, "xzero-logwriter");
#endif

    while (true) {
        void* ptr = nullptr;
        ssize_t rv = ::read(receiverFd_, &ptr, sizeof(ptr));

        if (rv < 0) {
            continue;
        }

        if (ptr == nullptr) {
            // cycle() writes a NULL into the stream to notify us to cycleNow().
            cycleNow();
            continue;
        }

        std::unique_ptr<Buffer> msg(reinterpret_cast<Buffer*>(ptr));
        rv = ::write(fileFd_, msg->data(), msg->size());

        if (rv < 0) {
            writeErrors_++;
        }
    }
}

} // namespace x0
