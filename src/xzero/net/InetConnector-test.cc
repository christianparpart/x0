// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/net/InetConnector.h>
#include <xzero/net/InetEndPoint.h>
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
  EchoServerConnection(EndPoint* endpoint, Executor* executor);
  void onOpen(bool dataReady) override;
  void onFillable() override;
  void onFlushable() override;
};

EchoServerConnection::EchoServerConnection(EndPoint* endpoint, Executor* executor)
    : Connection(endpoint, executor) {
}

void EchoServerConnection::onOpen(bool dataReady) {
  Connection::onOpen(dataReady);

  if (dataReady)
    onFillable();
  else
    wantFill();
}

void EchoServerConnection::onFillable() {
  Buffer inputBuffer;
  size_t n = endpoint()->fill(&inputBuffer);
  if (n == 0) {
    close();
  } else {
    endpoint()->setBlocking(true);
    endpoint()->flush(inputBuffer);
    endpoint()->setBlocking(false);
    wantFill();
  }
}

void EchoServerConnection::onFlushable() {
  // not needed, as we're doing blocking writes
}
// }}}
class EchoClientConnection : public xzero::Connection { // {{{
 public:
  EchoClientConnection(EndPoint* endpoint, Executor* executor,
                       const BufferRef& text,
                       std::function<void(const BufferRef&)> responder);
  void onOpen(bool dataReady) override;
  void onFillable() override;

 private:
  BufferRef text_;
  std::function<void(const BufferRef&)> responder_;
};

EchoClientConnection::EchoClientConnection(
    EndPoint* endpoint,
    Executor* executor,
    const BufferRef& text,
    std::function<void(const BufferRef&)> responder)
    : Connection(endpoint, executor),
      text_(text),
      responder_(responder) {
}

void EchoClientConnection::onOpen(bool dataReady) {
  size_t n = endpoint()->flush(text_);
  wantFill();
}

void EchoClientConnection::onFillable() {
  Buffer inputBuffer;
  size_t n = endpoint()->fill(&inputBuffer);
  close();
  responder_(inputBuffer);
}
// }}}

TEST(InetConnector, echoServer) {
  PosixScheduler sched;

  auto connectionFactory = [&](Connector* connector, EndPoint* ep) -> Connection* {
    return ep->setConnection<EchoServerConnection>(ep, &sched);
  };

  std::shared_ptr<InetConnector> connector = std::make_shared<InetConnector>(
      "inet",
      &sched,
      nullptr,
      5_seconds, // read timeout
      5_seconds, // write timeout
      0_seconds, // TCP_FIN timeout
      IPAddress("127.0.0.1"),
      InetConnector::RandomPort,
      128, // backlog
      false, // reuseAddr
      false); // reusePort

  connector->addConnectionFactory("test", connectionFactory);
  connector->start();
  logf("Listening on port $0", connector->port());

  Future<RefPtr<EndPoint>> f = InetEndPoint::connectAsync(
      InetAddress("127.0.0.1", connector->port()),
      5_seconds, 5_seconds, 5_seconds, &sched);

  Buffer response;
  auto onClientReceived = [&](const BufferRef& receivedText) {
    logf("Client received \"$0\"", receivedText);
    response = receivedText;
    connector->stop();
  };

  auto onConnectionEstablished = [&](RefPtr<EndPoint> ep) {
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

TEST(InetConnector, detectProtocols) {
  PosixScheduler sched;

  std::shared_ptr<InetConnector> connector = std::make_shared<InetConnector>(
      "inet",
      &sched,
      nullptr,
      5_seconds, // read timeout
      5_seconds, // write timeout
      0_seconds, // TCP_FIN timeout
      IPAddress("127.0.0.1"),
      InetConnector::RandomPort,
      128, // backlog
      false, // reuseAddr
      false); // reusePort

  int echoCreated = 0;
  auto echoFactory = [&](Connector* connector, EndPoint* ep) -> Connection* {
    echoCreated++;
    return ep->setConnection<EchoServerConnection>(ep, &sched);
  };

  int yeahCreated = 0;
  auto yeahFactory = [&](Connector* connector, EndPoint* ep) -> Connection* {
    yeahCreated++;
    return ep->setConnection<EchoServerConnection>(ep, &sched);
  };

  connector->addConnectionFactory("echo", echoFactory);
  connector->addConnectionFactory("yeah", yeahFactory);
  connector->start();
  logf("Listening on port $0", connector->port());

  Future<RefPtr<EndPoint>> f = InetEndPoint::connectAsync(
      InetAddress("127.0.0.1", connector->port()),
      5_seconds, 5_seconds, 5_seconds, &sched);

  Buffer response;
  auto onClientReceived = [&](const BufferRef& receivedText) {
    logf("Client received \"$0\"", receivedText);
    response = receivedText;
    connector->stop();
  };

  auto onConnectionEstablished = [&](RefPtr<EndPoint> ep) {
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
