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
HttpClient cli(&sched);

HttpRequestInfo req(HttpVersion::VERSION_1_1, "GET", "/", 0, {
    {"User-Agent", "raw/0.1"},
  });

cli.connect(IPAddress("127.0.0.1"), 8080);
cli.send(req);
```
