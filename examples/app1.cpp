/*! \brief Very simple HTTP server. Everything's done for you.
 *
 * It just serves static pages.
 */
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/io/BufferSource.h>
#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <ev++.h>

class MyServer {
private:
    struct ev_loop* loop_;
    ev::sig sigterm_;
    ev::sig sigint_;
    std::unique_ptr<x0::HttpServer> http_;

public:
    MyServer() :
        loop_(ev_default_loop()),
        sigterm_(loop_),
        sigint_(loop_),
        http_()
    {
        std::clog << "Initializing ..." << std::endl;

        sigterm_.set<MyServer, &MyServer::terminateHandler>(this);
        sigterm_.start(SIGTERM);
        ev_unref(loop_);

        sigint_.set<MyServer, &MyServer::terminateHandler>(this);
        sigint_.start(SIGINT);
        ev_unref(loop_);

        http_.reset(new x0::HttpServer(loop_));
        http_->requestHandler = std::bind(&MyServer::requestHandler, this, std::placeholders::_1);
        http_->setupListener("0.0.0.0", 3000);

        // run HTTP with 4 worker threads (1 main thread + 3 posix threads)
        while (http_->workers().size() < 4) {
            http_->createWorker();
        }
    }

    ~MyServer()
    {
        std::clog << "Quitting ..." << std::endl;

        if (sigterm_.is_active()) {
            ev_ref(loop_);
            sigterm_.stop();
        }

        if (sigint_.is_active()) {
            ev_ref(loop_);
            sigint_.stop();
        }
    }

    int run()
    {
        std::clog << "Listening on http://0.0.0.0:3000 ..." << std::endl;
        return http_->run();
    }

private:
    bool requestHandler(x0::HttpRequest* r)
    {
        if (r->method != "HEAD" && r->method != "GET") {
            r->status = x0::HttpStatus::MethodNotAllowed;
            r->finish();
            return true;
        }

        x0::Buffer body;
        body.push_back("Hello, World!\n");

        r->status = x0::HttpStatus::Ok;
        r->responseHeaders.push_back("Content-Type", "text/plain");
        r->responseHeaders.push_back("Content-Length", std::to_string(body.size()));
        r->responseHeaders.push_back("X-Worker-ID", std::to_string(http_->currentWorker()->id()));

        if (r->method != "HEAD") {
            r->write<x0::BufferSource>(std::move(body));
        }

        r->finish();

        return true;
    }

    void terminateHandler(ev::sig& sig, int)
    {
        std::clog << "Signal (" << sig.signum << ") received. Terminating." << std::endl;

        ev_ref(loop_);
        sig.stop();

        http_->stop();
    }
};

int main(int argc, const char *argv[])
{
    MyServer s;
    return s.run();
}
