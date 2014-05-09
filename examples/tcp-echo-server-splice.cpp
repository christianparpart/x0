#include <x0/io/Pipe.h>
#include <x0/ServerSocket.h>
#include <x0/Socket.h>
#include <memory>
#include <ev++.h>
#include <fcntl.h>
#include <stdio.h>

using namespace x0;

class EchoServer {
public:
    EchoServer(struct ev_loop* loop, const char* bind, int port);
    ~EchoServer();

    void stop();

private:
    void incoming(std::unique_ptr<Socket>&& client, ServerSocket* server);

    struct ev_loop* loop_;
    ServerSocket* ss_;

    class Session;
};

class EchoServer::Session {
public:
    explicit Session(Socket* client);
    ~Session();

    void close();

private:
    void io(Socket* /*client*/, int /*revents*/);

    Socket* client_;
    Pipe pipe_;
};

EchoServer::EchoServer(struct ev_loop* loop, const char* bind, int port) :
    loop_(loop),
    ss_(nullptr)
{
    ss_ = new ServerSocket(loop_);
    ss_->set<EchoServer, &EchoServer::incoming>(this);
    ss_->open(bind, port, O_NONBLOCK);

    ss_->start();

    printf("Listening on tcp://%s:%d ...\n", bind, port);
}

EchoServer::~EchoServer()
{
    printf("Shutting down.\n");
    delete ss_;
}

void EchoServer::stop()
{
    printf("Shutdown initiated.\n");
    ss_->stop();
}

void EchoServer::incoming(std::unique_ptr<Socket>&& client, ServerSocket* /*server*/)
{
    new Session(client.release());
}

EchoServer::Session::Session(Socket* client) :
    client_(client),
    pipe_()
{
    printf("client connected.\n");
    client_->setReadyCallback<EchoServer::Session, &EchoServer::Session::io>(this);
    client_->setMode(Socket::Read);
}

EchoServer::Session::~Session()
{
    printf("client disconnected.\n");
    delete client_;
}

void EchoServer::Session::close()
{
    delete this;
}

void EchoServer::Session::io(Socket* client, int revents)
{
    ssize_t n = client->read(&pipe_, 1024);

    if (n <= 0) {
        close();
    } else {
        client_->setNonBlocking(false);
        client_->write(&pipe_, n);
        client_->setNonBlocking(true);
    }
}

int main()
{
    ev::loop_ref loop = ev::default_loop();
    EchoServer echo(loop, "0.0.0.0", 7979);
    loop.run();
    return 0;
}
