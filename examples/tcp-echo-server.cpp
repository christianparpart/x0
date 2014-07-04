// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/Buffer.h>
#include <x0/ServerSocket.h>
#include <x0/Socket.h>
#include <x0/Logger.h>
#include <memory>
#include <ev++.h>
#include <fcntl.h>
#include <stdio.h>

using namespace x0;

class EchoServer {
 public:
  EchoServer(struct ev_loop* loop, const char* bind, int port, Logger* logger);
  ~EchoServer();

  void stop();

  Logger* logger() const { return logger_; }

 private:
  void incoming(std::unique_ptr<Socket>&& client, ServerSocket* server);

  template <typename... Args>
  void log(Severity severity, const char* fmt, Args... args);

  struct ev_loop* loop_;
  ServerSocket* ss_;
  Logger* logger_;

  class Session;
};

class EchoServer::Session {
 public:
  Session(EchoServer* service, Socket* client);
  ~Session();

  void close();

 private:
  void io(Socket* /*client*/, int /*revents*/);

  template <typename... Args>
  void log(Severity severity, const char* fmt, Args... args);

  EchoServer* service_;
  Socket* client_;
};

EchoServer::EchoServer(struct ev_loop* loop, const char* bind, int port,
                       Logger* logger)
    : loop_(loop), ss_(nullptr), logger_(logger) {
  ss_ = new ServerSocket(loop_);
  ss_->set<EchoServer, &EchoServer::incoming>(this);
  ss_->open(bind, port, O_NONBLOCK);

  ss_->start();

  log(Severity::notice, "Listening on tcp://%s:%d ...", bind, port);
}

EchoServer::~EchoServer() {
  log(Severity::notice, "Shutting down.");
  delete ss_;
}

template <typename... Args>
void EchoServer::log(Severity severity, const char* fmt, Args... args) {
  LogMessage msg(severity, fmt, args...);
  msg.addTag("service");
  logger_->write(msg);
}

void EchoServer::stop() {
  log(Severity::notice, "Shutdown initiated.");
  ss_->stop();
}

void EchoServer::incoming(std::unique_ptr<Socket>&& client,
                          ServerSocket* /*server*/) {
  new Session(this, client.release());
}

EchoServer::Session::Session(EchoServer* service, Socket* client)
    : service_(service), client_(client) {
  log(Severity::notice, "client connected.");
  client_->setReadyCallback<EchoServer::Session, &EchoServer::Session::io>(
      this);
  client_->setMode(Socket::Read);
}

EchoServer::Session::~Session() {
  log(Severity::notice, "client disconnected.");
  delete client_;
}

void EchoServer::Session::close() { delete this; }

template <typename... Args>
void EchoServer::Session::log(Severity severity, const char* fmt,
                              Args... args) {
  LogMessage msg(severity, fmt, args...);
  msg.addTag("%s:%d", client_->remoteIP().c_str(), client_->remotePort());
  service_->logger()->write(msg);
}

void EchoServer::Session::io(Socket* client, int revents) {
  Buffer buf;
  client->read(buf);

  if (buf.empty()) {
    close();
  } else {
    client->setNonBlocking(false);
    client->write(buf);
    client->setNonBlocking(true);

    BufferRef b = buf.trim();

    log(Severity::info, "echo: %s", b.str().c_str());

    if (equals(b, ".")) {
      close();
    } else if (equals(b, "..")) {
      service_->stop();
      close();
    }
  }
}

int main() {
  auto loop = ev_default_loop();
  std::unique_ptr<Logger> logger(new ConsoleLogger());
  logger->setLevel(Severity::debug);
  std::unique_ptr<EchoServer> es(
      new EchoServer(loop, "0.0.0.0", 7979, logger.get()));
  ev_run(loop);
  return 0;
}
