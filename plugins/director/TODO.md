- *GENERAL*
  - Thread Safety at a minimum of lock contention to scale horizontally.
- *Director*
  - DONE: loading of new (empty) directors
  - sticky offline mode
  - request queueing timeouts, requests being queued for too long
    should result into a 503 (Retry-After: %d (60), with a configurable %d).
  - HTB-based request shaping: create buckets of given {name,rate,ceil} and
    let requests be classified by certain tags, assigned via helper Flow-method,
    like: void director.classify_request(string bucket_name);
    or: void director.classify(string bucket_name);
    or: void req.classify(string bucket_name);
  - Saving mutable directors onto disk should not block a thread.
- *Backend*
  - configurable connection timeouts for: connect/read/write
  - historical request-per-second stats for the last N (60) seconds
    (possibly as dedicated object to collect the data)
- *HealthMonitor*
  - check mode: lazy
  - check mode: opportunistic
  - configurable connection timeouts for: connect/read/write
- *HttpHealthMonitor*
  - conn timeout handling
- *FastCgiHealthMonitor*
  - conn timeout handling
- *HttpBackend*
  - conn timeout handling
- *FastCgiBackend*
  - conn timeout handling
