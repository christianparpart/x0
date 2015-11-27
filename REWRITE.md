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
- [ ] (flow:bug) `"Blah #{call}blah#{call}"` doesn't work
      unless I specify it as `"Blah #{call()}blah#{call}"`
- [ ] (flow:bug) `listen(port: "80")` MUST raise a signature mismatch error

### proxy

- [x] HttpClient: basic HTTP client API with generic transport layer (HTTP1, ...)
- [x] HttpClusterScheduler
- [x] HttpClusterScheduler::RoundRobin
- [x] HttpClusterScheduler::Chance
- [x] flow-api: `proxy.http(ipaddr, port)`
- [x] x0d: 100-continue - pre-read HttpInput
- [x] webdav implements PUT method
- [x] proxy.http: support request body
- [x] HttpHealthMonitor: monitor an HTTP upstream
- [x] HttpClient: an easier way to connect to an endpoint AND issue the request
      (in one line)
- [x] HttpClusterMember, proxies to HttpClient, with constraints (such as capacity)
- [x] HttpCluster: basic load balancing to HTTP/1 upstreams
- [x] use InetAddress instead of `pair<IPAddress, Port>`
- [x] configuration load
- [x] configuration save
- [ ] JSON API: GET
- [ ] JSON API: PUT (create)
- [ ] JSON API: POST (update)
- [ ] JSON API: DELETE
- [ ] request body tmp stored on disk if larger than N bytes
- [ ] response body tmp stored on disk if larger than N bytes
- [ ] HttpInput: must be rewind()able, in order to be used multiple times
      e.g. required for retransmitting a request to multiple upstreams if prior
      failed.
- ...

#### proxy: stage 2

- [ ] HttpInput: offload into local tmpfile if payload > N bytes
- [ ] proxy: properly proxy 'Expect: 100-continue' (Expect-header must be forwarded)
- [ ] HttpClient: FastCGI
- [ ] HttpClient: support UNIX domain socket alongside with TCP/IP
- [ ] HttpHealthMonitor: FastCGI
- [ ] flow-api: `proxy.fcgi(ipaddr, port)`
- [ ] Executor::HandleRef -> `<xzero/Action.h>` or similar to make it more generic

### Feature Stories

- [ ] support Server Sent Events (HTTP SSE)
- [ ] port director plugin, yeah
- [ ] x0d-signals: graceful shutdown (INT, TERM)
- [ ] x0d-signals: logfile rotating (HUP)
- [ ] x0d-signals: all the others (USR1, USR2, ...?)
