# Director Load Balancer Plugin

The director plugin implements a highly scalable reverse proxy HTTP and FastCGI dynamic load balancer.

Features:
- Supporting different backend protocols (HTTP and FastCGI)
- Supporting different transport protocols (TCP over IPv4 and IPv6, and UNIX Domain Sockets)
- (HTB) Cluster Partitioning
- Advanced health monitoring
- JSON API for retrieving state, stats, and reconfiguring clusters at runtime (including adding/updating/removing backends)
- HAproxy compatibility API
- Client side routing support
- Sticky offline mode
- X-Sendfile support, full static file transmission acceleration
- Object Cache
  - fully runtime configurable
  - client-cache aware
  - life JSON stats
  - supports serving stale objects when no backend is available
  - Vary-response support.
  - multiple update strategies (locked, shadowed)


## Classes

- HealthMonitor
  - HttpHealthMonitor
  - FastCgiHealthMonitor
- Backend
  - HttpBackend
  - FastCgiBackend
- BackendCluster
- Scheduler
  - LeastLoadScheduler
  - RoundRobinScheduler
  - ChanceScheduler
- BackendManager
  - Director
  - RoadWarrior
- DirectorPlugin
- JsonApi
- HaproxyApi

## Thread Model

Each entity must be only modified within its designated worker thread.
This includes the request as well as the backend (including health monitor) and the director.
Every entity gets assigned a worker thread upon instanciation and
when one entity is to mutate the state of another, it has to invoke
the corresponding function from within the target's worker thread.

### Benifits

- We do not need any kind of locks, such as mutexes, etc. and thus no lock contention
  on high traffic with many worker threads..
- CPU cache locality (less cache misses)

### Drawbacks

- increased latency due to transferring the request across through multiple worker threads.

## Director Plugin Blueprints

### Scheduler API to *only* schedule

The Scheduler should only decide what backend to use next and not queue requests.
Furthermore, we might introduce a backend filter API that a request has to pass,
that filters out the backends that a request may be scheduled onto.

- class Scheduler
  - `Backend* Scheduler::schedule(HttpRequest* r);`
  - class Scheduler::RR         -- round-robin
  - class Scheduler::Chance     -- chance, pass to the first one that accepts it
  - class Scheduler::LeastLoad  -- pass to the first backend with the least load

- class BackendFilter
  - class BackendFilter::Flat
  - class BackendFilter::Classful

    for (backend: backendFilter_->filter(r))
        if (backend->handleRequest(r))
            return;

    backendFilter_.enqueue(r);

### Classful Scheduling

With classful request scheduling we provide a facilithy to ensure that certain reuests
get at least a given percentage of the allocated cluster. Say you have a shop application
and want to ensure that the checkout process is always preferred over the standard
page requests, such as viewing the homepage or the search page.

You devide your cluster into buckets, say you have a vip bucket that gets all checkouts and 
other important reuests and ensure that this bucket has 20% of the cluster capacity ensured
and may use up to 40%.

The main bucket, at which any other reuests are passed though, will thus get 80% ensured
and may use up to, say, 100% of the cluster capacity.

So each bucket may borrow from the other but has a certain percentage ensured, so no
bucket will suffice from starvation.

#### Implementation

    class Bucket

#### TODO

- upon backend creation, the scheduler's root bucket gets added %capacity to its ceiling.
- upon backend deletion, the scheduler's root bucket gets cut by backend's %capacity.
  - if ceilign can't be fully reduced by %capacity, then backend deletion MUST fail.

- make classful scheduler the default, and/or even think about only supporting
  this kind of queueing scheduler algorithm.

### Web Interface

- Overview-page (haproxy-alike), listing all clusters, and its backends with their stats, and allowing to enable/disable those
- Live-page showing live stats:
  - charts, and historical graphs of cluster/bucket/backend utilization
  - current reuest-per-second for all

