# GENERAL

- Thread Safety at a minimum of lock contention to scale horizontally.

# Director

- HTB-based request scheduler: create buckets of given {name,rate,ceil} and
  let requests be classified by certain tags, assigned via helper Flow-method,
  like: `void director.classify_request(string bucket_name);`
  or: `void director.classify(string bucket_name);`
  or: `void req.classify(string bucket_name);`
- Saving mutable directors onto disk should not block a thread.

# Backend

- historical request-per-second stats for the last N (60) seconds
  (possibly as dedicated object to collect the data)

# HealthMonitor

- check mode: lazy
- check mode: opportunistic
