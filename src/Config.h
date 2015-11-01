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
  int workers = 1;
  std::list<ListenerConfig> listeners;
  std::list<SslContext> sslContexts;

  std::string mimetypesPath;
  std::string mimetypesDefault = "application/octet-stream";

  size_t maxRequestUriLength = 1024;              // 1 KB
  size_t maxRequestHeaderSize = 8 * 1024;         // 8 KB
  size_t maxRequestHeaderCount = 128;
  size_t maxRequestBodySize = 16 * 1024 * 1024;   // 16 MB
  size_t requestHeaderBufferSize = 16 * 1024;     // 16 KB
  size_t requestBodyBufferSize = 16 * 1024;       // 16 KB
  size_t maxKeepAliveRequests = 100;
  xzero::Duration maxKeepAlive = xzero::Duration::fromSeconds(8);

  bool tcpCork = false;
  bool tcpNoDelay = false;
  size_t maxConnections = 1024;
  xzero::Duration maxReadIdle = xzero::Duration::fromSeconds(60);
  xzero::Duration maxWriteIdle = xzero::Duration::fromSeconds(360);
  xzero::Duration tcpFinTimeout = xzero::Duration::fromSeconds(60);
  xzero::Duration lingering = xzero::Duration::fromSeconds(0);

#if 0
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
