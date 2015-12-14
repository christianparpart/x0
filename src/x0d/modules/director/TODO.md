# HAproxy Compatibility

Support haproxy config syntax to load load HTTP clusters (others are
warned and ignored). They cannot be runtime configured nor saved;

- automatically ensure HTTP listeners for the registered ports.

```
handler setup {
  haproxy.load '/etc/haproxy.cfg';
}

handler main {
  accesslog '/var/log/x0d/access.log';
  haproxy.serve;
}
```

# Backend "slow-start"

`slow-start` is by default set to 0, which means, that as soon as this backend is marked online, it'll be hit with
the `min(backendCapacity,pendingRequests)`.
A slow start value greater than 0 allows the backend to warm up, e.g. JIT compile and populate other caches.

The question  whether we want this parameter per backend or per cluster.

# ObjectCache

- Do not cache conditional responses like 304's. those headers must be filtered out to the backend when the object cache is active.
  - or differently handled.
- Honor conditional requests to support client-side caching
  - If-Match
  - If-None-Match
  - If-Modified-Since
  - If-Unmodified-Since

# TokenShaper Integration

- Backend hooks on setEnabled & setCapacity should automatically resize the shaper's capacity.
  - but since Backend doesn't know anything about Director but BackendManager and this one must not know about shaper,
    we might need a callback here, so that Director can register at each backend to get notified about
    capacity/enabled state changes.
- TokenShaper + Director, does it play nice with multiple backend roles like Active/Standby/Backup ?
- ensure that the director (including backends?) are executed only from within one worker.

# Director On Demand

- Create a new cluster via `PUT /director/:name?hostname=NAME&port=PORT`.
- Access ondemand created directors via handler `director.ondemand;`.
- on-demand managed directors are by default stored in /var/lib/x0d/directors-ondemand/HOSTNAME.db
  - the base filename is insignificant.
  - contains the following additional infos that are used for director selection:
    - [ondemand] hostname
    - [ondemand] port

------------------------------------------------------------------------------

# GENERAL

- Thread Safety at a minimum of lock contention to scale horizontally.

# Director

- HTB-based request scheduler: create buckets of given {name,rate,ceil} and
  let requests be classified by certain tags, assigned via helper Flow-method,
  like: `void director.classify_request(string bucket_name);`
  or: `void director.classify(string bucket_name);`
  or: `void req.classify(string bucket_name);`
- Saving mutable directors onto disk should not block a thread.
- scheduler: support switching from one onto another.

# Scheduler

# Backend

- historical request-per-second stats for the last N (60) seconds
  (possibly as dedicated object to collect the data)

# HealthMonitor

- check mode: lazy
- check mode: opportunistic

# ClassfulScheduler

- root        backends.each{|b| b.capacity}.sum
  - VIP       5..20 %                               3
  - Upload    5..10 %                               7
  - Backend   1..10 t (tokens, aka. requests)       0
  - Jail      1..5 t                                2

# How to dequeue requests from their bucket queues

# Example Provisioning

  root:    VVVVV UUUUU B J vvvvvvvvvv uuuuu b j -------------------------------
  VIP:     vvvvv ..........
  Upload:  uuuuu .....
  Backend: b ...
  Jail:    j ...
  %rest%:  -------------------------------


overrate = max(0, actual - rate)
