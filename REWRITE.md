### Intermediate Tasks

- [ ] http: HttpListener, check if HugeBuffer for body passing does make sense (http2 prep)
- [ ] http1: move connection-headers into http1::Channel
      (removed from request headers)
- [ ] http1: provide a better (more generic) protocol-upgrade API
- [ ] fix license fuckup (different license headers in different files)
      And update year to ..2016 in all

- [ ] (make thread safe) File::lastModified()
- [x] (flow:bug) `"Blah #{call}blah#{call}"` doesn't work
      unless I specify it as `"Blah #{call()}blah#{call}"`
- [ ] (flow:bug) `listen(port: "80")` MUST raise a signature mismatch error
- [x] (flow) `var x = call1 + '.' + call2;` not working. fix me.
- [ ] (flow) tag flow handlers to never return (aka. always handle),
      thus, enabling the compiler to give a warning on dead code after
      this handler.

### Cleanup Tasks

- [ ] (test) ensure http1 keep-alive is working on the transport-level
- [ ] (test) ensure http1 pipelined processing
- [ ] (quality) HttpChannelState::DONE is questionable as we're switching
      back to SENDING after DONE, then to HANDLING again.

### Smallish Features

- [ ] (flow) a verifier callback must have the ability to attach custom data
      to the actual call.
      something like `CallInstr.attach(OwnedPtr<CustomData> data);

### Incomplete Migration Tasks

- [ ] revive `lingering`
- [ ] revive `max_connections`
- [ ] revive HTTP client side abort notification API
- [x] x0d-signals: config reload (HUP)
- [x] UnixSignals API
- [x] UnixSignals: OS/X (kqueue)
- [x] UnixSignals: Linux (signalfd)
- [x] PosixScheduler: add refCount in order to allow checking for interests
      (just like in libev), so it is easier for UnixSignals to silently
      watch without letting the event loop being stuck even no actual
      interest has been created.
- [x] x0d-signals: logfile rotating (USR1)
- [x] x0d-signals: graceful shutdown (QUIT)
- [x] x0d-signals: quick shutdown (INT, TERM)
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
