# Xzero C++ HTTP Library

`libxzero-http` is a thin low-latency, scalable, and embeddable HTTP server library
written in modern C++.

### Feature Highlights

- Composable API
- HTTP/1.1, including chunked trailer and pipelining support
- Generic core HTTP API with support for: HTTP (HTTPS, FCGI, HTTP/2, ...)
- Client-cache aware and partial static file serving
- zero-copy networking optimizations
- Unit Tests

## Installation Requirements

- `libxzero-base` including its dependencies

### Renaming

```
xzero {
  http {
    http1 {
      Channel = Http1Channel
      ConnectionFactory = Http1ConnectionFactory
      Connection = HttpConnection
      Generator = HttpGenerator
      Parser = HttpParser
    }
    HeaderField
    HeaderFieldList
    Channel
    ConnectionFactory
    DateGenerator
    File
    FileHandler
    Handler
    MessageInfo
    MessageListener
    RequestMethod
    OutputCompressor
    RangeDef
    Request
    RequestInfo
    Response
    ResponseInfo
    Service
    StatusCode
    Transport
    Version
  }
}
```

### Little Multiplex-Refactor

```
cortex::http::fastcgi::
  ConnectionFactory
  Connection
  RequestParser
  ResponseParser
  Generator
```

