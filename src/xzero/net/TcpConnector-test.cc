// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/net/TcpConnector.h>
#include <xzero/net/TcpEndPoint.h>
#include <xzero/net/InetAddress.h>
#include <xzero/net/Connection.h>
#include <xzero/net/IPAddress.h>
#include <xzero/executor/PosixScheduler.h>
#include <xzero/util/BinaryWriter.h>
#include <xzero/BufferUtil.h>
#include <xzero/Duration.h>
#include <memory>

using namespace xzero;

class EchoServerConnection : public xzero::Connection { // {{{
 public:
  EchoServerConnection(TcpEndPoint* endpoint, Executor* executor);
  void onOpen(bool dataReady) override;
  void onReadable() override;
  void onWriteable() override;
};

EchoServerConnection::EchoServerConnection(TcpEndPoint* endpoint, Executor* executor)
    : Connection(endpoint, executor) {
}

void EchoServerConnection::onOpen(bool dataReady) {
  Connection::onOpen(dataReady);

  if (dataReady)
    onReadable();
  else
    wantRead();
}

void EchoServerConnection::onReadable() {
  Buffer inputBuffer;
  size_t n = endpoint()->read(&inputBuffer);
  if (n == 0) {
    close();
  } else {
    endpoint()->setBlocking(true);
    endpoint()->write(inputBuffer);
    endpoint()->setBlocking(false);
    wantRead();
  }
}

void EchoServerConnection::onWriteable() {
  // not needed, as we're doing blocking writes
}
// }}}
class EchoClientConnection : public xzero::Connection { // {{{
 public:
  EchoClientConnection(TcpEndPoint* endpoint, Executor* executor,
                       const BufferRef& text,
                       std::function<void(const BufferRef&)> responder);
  void onOpen(bool dataReady) override;
  void onReadable() override;

 private:
  BufferRef text_;
  std::function<void(const BufferRef&)> responder_;
};

EchoClientConnection::EchoClientConnection(
    TcpEndPoint* endpoint,
    Executor* executor,
    const BufferRef& text,
    std::function<void(const BufferRef&)> responder)
    : Connection(endpoint, executor),
      text_(text),
      responder_(responder) {
}

void EchoClientConnection::onOpen(bool dataReady) {
  size_t n = endpoint()->write(text_);
  wantRead();
}

void EchoClientConnection::onReadable() {
  Buffer inputBuffer;
  size_t n = endpoint()->read(&inputBuffer);
  close();
  responder_(inputBuffer);
}
// }}}

auto EH(xzero::testing::Test* test) {
  return [test](const std::exception& e) {
    test->reportUnhandledException(e);
  };
}

TEST(TcpConnector, echoServer) {
  PosixScheduler sched(EH(this));

  auto connectionFactory = [&](TcpConnector* connector, TcpEndPoint* ep) -> Connection* {
    return ep->setConnection<EchoServerConnection>(ep, &sched);
  };

  std::shared_ptr<TcpConnector> connector = std::make_shared<TcpConnector>(
      "inet",
      &sched,
      nullptr,
      5_seconds, // read timeout
      5_seconds, // write timeout
      0_seconds, // TCP_FIN timeout
      IPAddress("127.0.0.1"),
      TcpConnector::RandomPort,
      128, // backlog
      false, // reuseAddr
      false); // reusePort

  connector->addConnectionFactory("test", connectionFactory);
  connector->start();
  logf("Listening on port $0", connector->port());

  Future<RefPtr<TcpEndPoint>> f = TcpEndPoint::connect(
      InetAddress("127.0.0.1", connector->port()),
      5_seconds, 5_seconds, 5_seconds, &sched);

  Buffer response;
  auto onClientReceived = [&](const BufferRef& receivedText) {
    logf("Client received \"$0\"", receivedText);
    response = receivedText;
    connector->stop();
  };

  auto onConnectionEstablished = [&](RefPtr<TcpEndPoint> ep) {
    ep->setConnection<EchoClientConnection>(ep.get(), &sched, "ping", onClientReceived);
    ep->connection()->onOpen(false);
  };

  std::error_code connectError;
  f.onFailure([&](std::error_code ec) { connectError = ec; });
  f.onSuccess(onConnectionEstablished);

  sched.runLoop();

  ASSERT_FALSE(connectError);
  EXPECT_EQ("ping", response);
}

TEST(TcpConnector, detectProtocols) {
  PosixScheduler sched;

  std::shared_ptr<TcpConnector> connector = std::make_shared<TcpConnector>(
      "inet",
      &sched,
      nullptr,
      5_seconds, // read timeout
      5_seconds, // write timeout
      0_seconds, // TCP_FIN timeout
      IPAddress("127.0.0.1"),
      TcpConnector::RandomPort,
      128, // backlog
      false, // reuseAddr
      false); // reusePort

  int echoCreated = 0;
  auto echoFactory = [&](TcpConnector* connector, TcpEndPoint* ep) -> Connection* {
    echoCreated++;
    return ep->setConnection<EchoServerConnection>(ep, &sched);
  };

  int yeahCreated = 0;
  auto yeahFactory = [&](TcpConnector* connector, TcpEndPoint* ep) -> Connection* {
    yeahCreated++;
    return ep->setConnection<EchoServerConnection>(ep, &sched);
  };

  connector->addConnectionFactory("echo", echoFactory);
  connector->addConnectionFactory("yeah", yeahFactory);
  connector->start();
  logf("Listening on port $0", connector->port());

  Future<RefPtr<TcpEndPoint>> f = TcpEndPoint::connect(
      InetAddress("127.0.0.1", connector->port()),
      5_seconds, 5_seconds, 5_seconds, &sched);

  Buffer response;
  auto onClientReceived = [&](const BufferRef& receivedText) {
    logf("Client received \"$0\"", receivedText);
    response = receivedText;
    connector->stop();
  };

  auto onConnectionEstablished = [&](RefPtr<TcpEndPoint> ep) {
    log("onConnectionEstablished");
    Buffer text;
    connector->loadConnectionFactorySelector("yeah", &text);
    text.push_back("blurrrb");
    ep->setConnection<EchoClientConnection>(ep.get(), &sched, text, onClientReceived);
    ep->connection()->onOpen(false);
  };

  std::error_code connectError;
  f.onFailure([&](std::error_code ec) { connectError = ec; });
  f.onSuccess(onConnectionEstablished);

  sched.runLoop();

  ASSERT_FALSE(connectError);
  EXPECT_EQ(1, yeahCreated);
  EXPECT_EQ(0, echoCreated);
  EXPECT_EQ("blurrrb", response);
}
