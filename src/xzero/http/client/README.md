## HttpClient Requirements

- streaming request
- streaming response
- concurrent request processing
  - http1: pipelined
  - http2: multiplexed
  - fastcgi: multiplexed
- API for blocking request/response challenge

## HttpClient API

- [ ] Unit Tests
- [x] Transport-Layer Transparent Client API
- [ ] sync, async, nonblocking I/O aware
- [ ] HTTP/1 Transport Layer
- [ ] FastCGI Transport Layer
- [ ] HTTP/2 Transport Layer
- [ ] SSL

### Example Usage

```
NativeScheduler sched;
HttpClient cli(&sched, InetAddress("127.0.0.1", 8080));

HttpRequest req(HttpVersion::VERSION_1_1, "GET", "/", 0, {
    {"User-Agent", "raw/0.1"},
  });

auto f = cli.send(req); // enqueues, in case
f.onSuccess([](const auto& response) { logDebug(response); });
f.onFailure([](auto error) { logDebug(error); });

