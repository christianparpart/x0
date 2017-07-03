
### Feature Check-List

- [ ] Supporting different backend transports (TCP/IP, and UNIX Domain Sockets).
- [ ] Supporting different backend protocols (HTTP and FastCGI)
- [ ] Advanced Health Monitoring
  [ ] and reconfiguring clusters (including adding/updating/removing backends).
- [ ] [Client Side Routing support](http://xzero.io/#/article/client-side-routing)
- [ ] Sticky Offline Mode
- [ ] X-Sendfile support, full static file transmission acceleration
- [ ] HAproxy compatible CSV output
- [ ] cache: general purpose HTTP response object cache
- [ ] cache: fully runtime configurable
- [ ] cache: client-cache aware
- [ ] cache: life JSON stats
- [ ] cache: supports serving stale objects when no backend is available
- [ ] cache: `Vary` response-header Support.
- [ ] cache: multiple update strategies (locked, shadowed)
- [ ] support custom error pages
- [ ] JSON API for retrieving state, stats, and reconfiguring clusters (including adding/updating/removing backends).

### relevant APIs

- `HttpClient` HTTP client
- `HttpReverseProxy` HTTP reverse proxy for simple use within a request handler (TODO)
- `HttpCluster` HTTP load balancer over a cluster of HTTP upstreams
- `HttpClusterMember`
- `HttpClusterMember::EventListener`
- `HttpClusterScheduler`
- `HttpClusterScheduler::RoundRobin`
- `HttpClusterScheduler::Chance`
- `HttpClusterScheduler::LeastLoad` TODO
- `HttpCache`

### References

* [What Proxies Must Do](https://www.mnot.net/blog/2011/07/11/what_proxies_must_do)
