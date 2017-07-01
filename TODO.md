
## Incomplete TODO items

### Intermediate 0

- [ ] test: ensure `HttpStatus::NoResponse` actually terminates the transport instant
- [x] *fix* SIGNALS section in x0d man page. bring it up to date.
- Proper Error Page Handling
  - [x] ensure global status code maps are also looked up (secondary)
  - [x] `error.page(int status, string internal_uri, int override = 0)`
        to support internal URI redirection upon given status codes;
        Allows internal redirection by running main handler again with
        request method overridden as GET and request URI set to the above.
        If `override` is set to a status code, that one will be sent out
        instead.
  - [x] consider dropping `error.handler()` completely (until reintroduced)
  - [x] `error.page(status, external_uri)` to support external URI redirects upon given status codes
        That means, it'll respond with a 302 (default) or any other 30x status
        code and adds a Location response header.
  - [x] ensure `staticfile` honors error pages
  - [x] ensure `precompressed` honors error pages
- SslConnector: add optional support to also allow plaintext connections
- BUG: testing: EXPECTxx failures do also increment success count?
- extend FCGI connector to tweak maxKeepAlive (just like in HTTP/1)
  - make sure the keepalive-timeout is fired correctly
    - test cancellation (due to io) and fire (due to timeout).
- use wireshark to get binary expected data for the Parser & Generator tests
- easy enablement of FCGI in all http examples.

### Milestone 1

* [x] LinuxScheduler (`epoll`, `signalfd`, `eventfd`)
- [x] FCGI transport
- [x] SSL: basic `SslConnector` & `SslEndPoint`
- [x] SSL: finish SNI support
- [x] SSL: NPN support
- [x] SSL: ALPN support
- [x] SSL: SslConnector to select ConnectionFactory based on NPN/ALPN
- [x] UdpConnector
- [x] improve timeout management (ideally testable)

### Milestone 2

- [ ] SSL: ability to setup a certificate password challenge callback
- [ ] Process (QA: prefer `posix_spawn()` over `vfork()` to exec child processes)
- [ ] write full tests for HttpFileHandler using MockTransport
- [ ] net: improved EndPoint timeout handling
      (distinguish between read/write/keepalive timeouts)
- [ ] `HttpTransport::onInterestFailure()` => `(factory || connector)->report(this, error);`
- [x] `InetEndPoint::wantFill()` to honor `TCP_DEFER_ACCEPT`
- [ ] chunked request trailer support & unit test
- [ ] doxygen: how to document a group of functions all at once (or, how to copydoc)
- [ ] test: call completed() before contentLength is satisfied in non-chunked mode (shall be transport generic)
- [ ] test: attempt to write more data than contentLength in non-chunked mode (shall be transport generic)
- [ ] improve (debug) logging facility

### General

- [ ] SSL: support different session cache stores (memory, memcached, ...)
- [ ] SSL: support enabling/disabling SSL renegotiation - http://www.thc.org/thc-ssl-dos/
- [ ] SSL: ensure we can reconfigure cipher priorities to counter BEAST attack - http://www.g-loaded.eu/2011/09/27/mod_gnutls-rc4-cipher-beast/
- [ ] http: ensure `TCP_CORK` / `TCP_NOPUSH` is set/cleared implicitely in plaintext vs interactive http responses

### HTTP load balancer

- [ ] raise 504 (gateway timeout) if backend page load takes too long and no result has been sent to the client yet (close connection directly otherwise).

### Flow
- [ ] SSA: mem2reg propagation (pass manager stage: before, one-time)
- [ ] SSA: reg2mem propagation (pass manager stage: after, one-time)
- [ ] SSA: constant propagation (pass manager stage: main)

### x0d

- [ ] stdout/stderr to point to log stream/handler directly,
- [ ] add `x0d --dump-du` (dump flow's def-use chain)
- [ ] resource management (at least for `max_core_size`) must be evaluated *after* privilege dropping, in case there is some, or they're lost.

