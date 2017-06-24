// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Option.h>
#include <xzero/Duration.h>
#include <xzero/net/IPAddress.h>
#include <xzero/http/HttpStatus.h>
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
  bool deferAccept;
  bool reusePort;
  bool ssl;
};

struct SslContext {
  std::string certfile;
  std::string keyfile;
  std::string trustfile;
  std::string priorities;
};

struct Config {
  size_t workers = 1;
  std::vector<int> workerAffinities;

  std::list<ListenerConfig> listeners;
  std::list<SslContext> sslContexts;

  std::string mimetypesPath;
  std::string mimetypesDefault = "application/octet-stream";

  size_t maxInternalRedirectCount = 3;

  size_t maxRequestUriLength = 1024;              // 1 KB
  size_t maxRequestHeaderSize = 8 * 1024;         // 8 KB
  size_t maxRequestHeaderCount = 128;
  size_t maxRequestBodySize = 16 * 1024 * 1024;   // 16 MB
  size_t requestHeaderBufferSize = 16 * 1024;     // 16 KB
  size_t requestBodyBufferSize = 16 * 1024;       // 16 KB
  size_t responseBodyBufferSize = 4 * 1024 * 1024;// 4 MB
  size_t maxKeepAliveRequests = 100;
  xzero::Duration maxKeepAlive = 8_seconds;

  bool tcpCork = false;
  bool tcpNoDelay = false;
  size_t maxConnections = 1024;
  xzero::Duration maxReadIdle = 60_seconds;
  xzero::Duration maxWriteIdle = 360_seconds;
  xzero::Duration tcpFinTimeout = 0_seconds;
  xzero::Duration lingering = 0_seconds;

  std::unordered_map<xzero::http::HttpStatus, std::string> errorPages;

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
