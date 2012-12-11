## Classes

- BackendManager
  - Director
  - RoadWarrior
- Scheduler
  - LeastLoadScheduler
  - ClassfulScheduler
- Backend
  - HttpBackend
  - FastCgiBackend
- HealthMonitor
  - HttpHealthMonitor
  - FastCgiHealthMonitor
- DirectorPlugin
- ApiRequest

## Director Plugin Blueprints

### Unification of proxy/fastcgi/director Plugins

We currently have quite a big chunk of code duplication inside
director plugin that mirrors the core functionality of proxy and fastcgi plugins.

We can mitigate this by deprecating proxy/fastcgi plugins and
add the ability to quickly pass a request to a floating backend w/o any
complex load balancing features, such as health checking & queueing.

This should be done by all means and at all cost without loosing too much performance.

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

#### TODO

- upon backend creation, the scheduler's root bucket gets added %capacity to its ceiling.
- upon backend deletion, the scheduler's root bucket gets cut by backend's %capacity.
  - if ceilign can't be fully reduced by %capacity, then backend deletion MUST fail.

- make classful scheduler the default, and/or even think about only supporting
  this kind of queueing scheduler algorithm.

