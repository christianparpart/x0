### Migration Tasks

- [x] access to Scheduler API from within HttpRequest or XzeroContext;
      consider merging `Scheduler` and `Executor`;
      consider using `Scheduler` instead of `Executor` in net and http code;
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
- [x] pull InputStream/OutputStream API from stx (away from istream/ostream)?
- [x] (flow) String interpolation doesn't allow function calls without ()'s.
      Example: "Welcome on ${sys.hostname}" is not working,
      but the: "Welcome on ${sys.hostname()}" is working.
- [x] revive `max_request_body`
- [x] HttpRequest: reuse of HttpRequestInfo
- [ ] HttpResponse: reuse of HttpResponseInfo (NEEDED?)
- [ ] revive `lingering`
- [ ] revive `max_connections`
- [ ] (make thread safe) File::lastModified()
- [ ] how to convert from UnixTime to CivilTime to get the current
- [ ] FileRef to be renamed to FileHandle/FileHandle or alike?
- [ ] (flow) `var x = call1 + '.' + call2;` not working. fix me.
- [ ] revive HTTP client side abort notification API
- [ ] console logger to also log timestamps, can be disabled (enabled by default)
- [ ] (flow) tag flow handlers to never return (aka. always handle),
      thus, enabling the compiler to give a warning on dead code after
      this handler.
- [ ] (bug) timeout problem in InetEndPoint, found via `xurl -> x0d -> local`

### Proxy Plugin

- [x] HttpClient: basic HTTP client API with generic transport layer (HTTP1, ...)
- [x] HttpClusterScheduler
- [x] HttpClusterScheduler::RoundRobin
- [x] HttpClusterScheduler::Chance
- [x] flow-api: `proxy.http(ipaddr, port)`
- [x] x0d: 100-continue - pre-read HttpInput
- [x] webdav implements PUT method
- [x] proxy.http: support request body
- [ ] HttpHealthMonitor: monitor an HTTP upstream (transport protocol independant)
  - paranoid
  - opportunistic
  - lazy
- [ ] HttpClusterMember, proxies to HttpClient, with constraints (such as capacity)
- [ ] HttpCluster: basic load balancing to HTTP/1 upstreams

#### proxy: stage 2

- [ ] flow-api: `proxy.fcgi(ipaddr, port)`
- [ ] HttpInput: offload into local tmpfile if payload > N bytes
- [ ] Executor::HandleRef -> `<xzero/Action.h>` or similar to make it more generic
- [ ] HttpClient: support UNIX domain socket alongside with TCP/IP
- [ ] proxy: properly proxy 'Expect: 100-continue' (Expect-header must be forwarded)
- [ ] HttpClient: support FastCGI

### Feature Stories

- [ ] port director plugin, yeah
- [ ] port stx::HTTPConnectionPool into HttpClient / HttpClientProxy
- [ ] x0d-signals: graceful shutdown (INT, TERM)
- [ ] x0d-signals: logfile rotating (HUP)
- [ ] x0d-signals: all the others (USR1, USR2, ...?)
