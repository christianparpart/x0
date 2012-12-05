## Classes

- BackendManager
  - Director
- Scheduler
  - LeastLoadScheduler
  - ClassfulScheduler
- Backend
  - HttpBackend
  - FastCgiBackend
- HealthMonitor
  - HttpHealthMonitor
  - FastCgiHealthMonitor

## Director Plugin Blueprints

### Unification of proxy/fastcgi/director Plugins

We currently have quite a big chunk of code duplication inside
director plugin that mirrors the core functionality of proxy and fastcgi plugins.

We can mitigate this by deprecating proxy/fastcgi plugins and
add the ability to quickly pass a request to a floating backend w/o any
complex load balancing features, such as health checking & queueing.

This should be done by all means and at all cost without loosing too much performance.

### Classful Scheduling

#### TODO

- upon backend creation, the scheduler's root bucket gets added %capacity to its ceiling.
- upon backend deletion, the scheduler's root bucket gets cut by backend's %capacity.
  - if ceilign can't be fully reduced by %capacity, then backend deletion MUST fail.

- make classful scheduler the default, and/or even think about only supporting
  this kind of queueing scheduler algorithm.

