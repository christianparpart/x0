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
