
## Incomplete TODO items

- eliminate DataChainListener and/or greatly simplify DataChain
- eliminate need of EndPointWriter via DataChain
- eliminate TcpUtil?
- eliminate SslUtil?
- finish SslClient
  - migrate into SslEndPoint

### Intermediate 0

- [ ] HttpListener: onMessagecontent(HugeBuffer&&) instead
- [ ] extend FCGI connector to tweak maxKeepAlive (just like in HTTP/1)
  - make sure the keepalive-timeout is fired correctly
    - test cancellation (due to io) and fire (due to timeout).
- [ ] http2: use wireshark to get binary expected data for the Parser & Generator tests
- [ ] easy enablement of FCGI in all http examples.

### General

- [ ] SSL: SslConnector: add optional support to also allow plaintext connections
- [ ] SSL: ability to setup a certificate password challenge callback
- [ ] SSL: support different session cache stores (memory, memcached, ...)
- [ ] SSL: support enabling/disabling SSL renegotiation - http://www.thc.org/thc-ssl-dos/
- [ ] SSL: ensure we can reconfigure cipher priorities to counter BEAST attack - http://www.g-loaded.eu/2011/09/27/mod_gnutls-rc4-cipher-beast/
- [ ] http: ensure `TCP_CORK` / `TCP_NOPUSH` is set/cleared implicitely in plaintext vs interactive http responses

### HTTP load balancer (catch-up, rewrite, testing, polishing)

- [ ] test: test each public API is well tested
- [ ] raise 504 (gateway timeout) if backend page load takes too long and no result has been sent to the client yet (close connection directly otherwise).

### Flow
- [ ] SSA: mem2reg propagation (pass manager stage: before, one-time)
- [ ] SSA: reg2mem propagation (pass manager stage: after, one-time)
- [ ] SSA: constant propagation (pass manager stage: main)

### x0d

- [ ] stdout/stderr to point to log stream/handler directly,
- [ ] add `x0d --dump-du` (dump flow's def-use chain)
- [ ] resource management (at least for `max_core_size`) must be evaluated *after* privilege dropping, in case there is some, or they're lost.

### Musclelicious

- [ ] core: Process (QA: prefer `posix_spawn()` over `vfork()` to exec child processes)
- [ ] net: improved EndPoint timeout handling (distinguish between read/write/keepalive timeouts)
- [ ] `HttpTransport::onInterestFailure()` => `(factory || connector)->report(this, error);`
- [ ] http: chunked request trailer support & unit test
- [ ] doc: doxygen: how to document a group of functions all at once (or, how to copydoc)
- [ ] test: call completed() before contentLength is satisfied in non-chunked mode (shall be transport generic)
- [ ] test: attempt to write more data than contentLength in non-chunked mode (shall be transport generic)
- [ ] logging: improve (debug) logging facility

### ALREADY DONE

- [x] BUG: testing: EXPECTxx failures do also increment success count?
- [x] test: ensure `HttpStatus::NoResponse` actually terminates the transport instant
- [x] redesigned error handling
- [x] LinuxScheduler (`epoll`, `signalfd`, `eventfd`)
- [x] FCGI transport
- [x] SSL: basic `SslConnector` & `SslEndPoint`
- [x] SSL: finish SNI support
- [x] SSL: NPN support
- [x] SSL: ALPN support
- [x] SSL: SslConnector to select ConnectionFactory based on NPN/ALPN
- [x] UdpConnector
- [x] improve timeout management (ideally testable)
- [x] `InetEndPoint::wantFill()` to honor `TCP_DEFER_ACCEPT`
- [x] test: write full tests for HttpFileHandler using MockTransport

