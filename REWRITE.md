### Migration Tasks

- [ ] HttpRequest: reuse of HttpRequestInfo
- [ ] HttpResponse: reuse of HttpResponseInfo
- [x] migrate DateTime to UnixTime/CivilTime
- [x] backport PosixScheduler changes from stx
- [x] make use of MonotonicTimer/Clock
- [x] fix logging
- [x] compose SafeCall over inheritance
- [x] threading: revive basic threaded non-blocking
- [x] threading: revive `SO_REUSEPORT`
- [x] revive `max_read_idle`
- [x] revive `max_write_idle`
- [x] revive `tcp_fin_timeout`
- [x] revive `tcp_cork`
- [x] revive `tcp_nodelay`
- [ ] revive `lingering`
- [ ] revive `max_connections`
- [x] pull InputStream/OutputStream API from stx (away from istream/ostream)?
- [ ] (make thread safe) File::lastModified()
- [ ] how to convert from UnixTime to CivilTime to get the current
- [ ] FileRef to be renamed to FileHandle/FileHandle or alike?
- [x] (flow) String interpolation doesn't allow function calls without ()'s.
      Example: "Welcome on ${sys.hostname}" is not working,
      but the: "Welcome on ${sys.hostname()}" is working.
- [ ] (flow) `var x = call1 + '.' + call2;` not working. fix me.
- [ ] revive HTTP client side abort notification API

### Proxy Plugin

- [x] HttpClient: basic HTTP client API with generic transport layer (HTTP1, ...)
- [ ] HttpClient: support FastCGI
- [ ] HttpClient: support UNIX domain socket alongside with TCP/IP
- [ ] flow-api: `proxy.http(ipaddr, port)`
- [ ] flow-api: `proxy.fcgi(ipaddr, port)`
- [x] HttpClusterScheduler
- [x] HttpClusterScheduler::RoundRobin
- [x] HttpClusterScheduler::Chance
- [ ] HttpClusterMember, proxies to HttpClient, with constraints (such as capacity)
- [ ] HttpCluster: basic load balancing to HTTP/1 upstreams
- [ ] HttpHealthCheck: monitor an HTTP upstream (transport protocol independant)

### Feature Stories

- [ ] port director plugin, yeah
- [ ] port stx::HTTPConnectionPool into HttpClient / HttpClientProxy
- [ ] x0d-signals: graceful shutdown (INT, TERM)
- [ ] x0d-signals: logfile rotating (HUP)
- [ ] x0d-signals: all the others (USR1, USR2, ...?)
