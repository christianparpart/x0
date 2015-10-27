// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/Option.h>
#include <xzero/Duration.h>
#include <xzero/net/IPAddress.h>
#include <list>
#include <unordered_map>
#include <string>

namespace x0d {

struct ListenerConfig {
  xzero::IPAddress bindAddress;
  int port;
  int backlog;
  int multiAcceptCount;
  int reuseAddr;
  int reusePort;
  bool ssl;
};

struct SslContext {
  std::string certfile;
  std::string keyfile;
  std::string trustfile;
  std::string priorities;
};

struct Config {
  std::list<ListenerConfig> listeners;
  std::list<SslContext> sslContexts;

#if 0
  std::string mimetypesPath;
  std::string mimetypesDefault;

  bool advertise_;
  size_t maxRequestHeaderSize;
  size_t maxRequestHeaderCount;
  size_t maxRequestBodySize;
  size_t requestHeaderBufferSize;
  size_t requestBodyBufferSize;
  size_t maxKeepAliveRequests;
  xzero::Duration maxKeepAlive;

  bool tcpCork;
  bool tcpNoDelay;
  size_t maxConnections;
  xzero::Duration maxReadIdle;
  xzero::Duration maxWriteIdle;
  xzero::Duration tcpFinTimeout;
  xzero::Duration lingering;

  // accesslog
  std::unordered_map<std::string, std::string> accesslogFormats;

  // compress
  std::list<std::string> compressTypes;
  size_t compressMinSize;
  size_t compressMaxSize;
  size_t compressLevel;

  // userdir
  std::string userdirName; // such as "public_html"
#endif
};

//void difference(const Config& a, const Config& b, Config* adds, Config* removes);

} // namespace x0d
