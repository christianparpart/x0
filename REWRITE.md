### Incomplete Migration Tasks

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
- [x] revive `request_header_buffer_size`
- [x] revive `request_body_buffer_size`
- [x] pull InputStream/OutputStream API from stx (away from istream/ostream)?
- [x] (flow) String interpolation doesn't allow function calls without ()'s.
      Example: "Welcome on ${sys.hostname}" is not working,
      but the: "Welcome on ${sys.hostname()}" is working.
- [x] revive `max_request_body`
- [x] console logger to also log timestamps, can be disabled (enabled by default)
- [x] HttpRequest: reuse of HttpRequestInfo
- [x] webdav implements PUT method
- [ ] x0d-signals: logfile rotating (HUP)
- [ ] x0d-signals: graceful shutdown (INT, TERM)
- [ ] x0d-signals: all the others (USR1, USR2, ...?)
- [ ] revive `lingering`
- [ ] revive `max_connections`
- [ ] revive HTTP client side abort notification API
- [ ] (make thread safe) File::lastModified()
- [ ] (flow:bug) `"Blah #{call}blah#{call}"` doesn't work
      unless I specify it as `"Blah #{call()}blah#{call}"`
- [ ] (flow:bug) `listen(port: "80")` MUST raise a signature mismatch error
- [ ] (flow) `var x = call1 + '.' + call2;` not working. fix me.
- [ ] (flow) tag flow handlers to never return (aka. always handle),
      thus, enabling the compiler to give a warning on dead code after
      this handler.
- [ ] (flow) a verifier callback must have the ability to attach custom data
      to the actual call.
      something like `CallInstr.attach(OwnedPtr<CustomData> data);

### proxy

- [x] HttpClient: basic HTTP client API with generic transport layer (HTTP1, ...)
- [x] HttpClusterScheduler
- [x] HttpClusterScheduler::RoundRobin
- [x] HttpClusterScheduler::Chance
- [x] flow-api: `proxy.http(ipaddr, port)`
- [x] x0d: 100-continue - pre-read HttpInput
- [x] proxy.http: support request body
- [x] HttpHealthMonitor: monitor an HTTP upstream
- [x] HttpClient: an easier way to connect to an endpoint AND issue the request
      (in one line)
- [x] HttpClusterMember, proxies to HttpClient, with constraints (such as capacity)
- [x] HttpCluster: basic load balancing to HTTP/1 upstreams
- [x] use InetAddress instead of `pair<IPAddress, Port>`
- [x] configuration load
- [x] configuration save
- [x] JSON API: CRUD cluster
- [x] JSON API: CRUD backend
- [x] JSON API: CRUD bucket
- [x] support TRACE method for `proxy.cluster`
- [x] support TRACE method for `proxy.http`
- [x] request body tmp stored on disk if larger than N bytes
- [x] response body tmp stored on disk if larger than N bytes
- [x] load/save `health-check-success-threshold`
- [x] support request bodies
- [x] fix request enqueuing/dequeueing
- [x] upstream response body's max-buffer-size should be configurable
      as a per-cluster config variable. this value is used in `HugeBuffer`.
      Default: 4 mbyte.
- ...

#### proxy: stage 2

- [ ] proxy: properly proxy 'Expect: 100-continue' (Expect-header must be forwarded)
- [ ] HttpClient: FastCGI
- [ ] HttpHealthMonitor: FastCGI
- [ ] flow-api: `proxy.fcgi(ipaddr, port)`
- [ ] HttpClient: support UNIX domain socket alongside with TCP/IP
- [ ] Executor::HandleRef -> `<xzero/Action.h>` or similar to make it more generic
- [ ] support disabling caching of upstream response bodies but perform
      slow I/O instead. make this feaure per-cluster runtime configurable.
      by default: caching is enabled.

### Feature Stories

- [ ] support Server Sent Events (HTTP SSE)
- [ ] proxy: idempotent HTTP requests should be retried when backend
      returned a 5xx, too.
      configure option: `proxy.retry_idempotent_on_5xx(B)V`

