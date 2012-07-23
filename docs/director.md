# director - x0 Load Balancing Plugin

# Requirements

- support multiple backend protocols
  - HTTP (via TCP and UNIX domain sockets)
  - FastCGI (via TCP and UNIX domain sockets)
  - ideally costom Flow handlers
- active/standby backend modes, where standby backends get only used when all active
  backends are at its capacity limits (and/or offline/disabled).
- request queue with a per-director limit, used when no active nor standby backend can currently process 
- support (per director) connect/read/write timeouts to backend
- support retrying requests and per-director retry-limits
- sticky offline mode
  (when a backend becomes offline, it gets disabled, too,
  so that it won't be used again, once being back online)
- support dynamic directors
  - adding/removing/editing backends at runtime
  - storing backend configuration in some plain file in /var/lib/x0/director/$NAME.db
- provide admin JSON-API for browsing, controlling directors, its backends, and their states/stats.
- reuse already existing code as used in `proxy` and `fastcgi` plugins.
  - existing plugins (proxy, fastcgi) should now reuse the new core API

# Transparent Backend Transport API

The *director* API should support multiple different transport layers, such as
HTTP and FastCGI over TCP/IP but also UNIX domain sockets, in case some backend
service is running locally.

# Resource Excaustion

## Backend Concurrency Overloading

If the server receives more requests than any *online* *active* marked
backend can currently handle, *online* *standby* marked backends are used
to take over.

If even no *online* *standby* marked backend is available to serve the
incoming request, it gets queued and is processed either as soon as a backend
becomes available again, or its client receives a timeout response if 
no backend became available in time.

If the queue becomes full, with a per-director set limit, the server responds
with a 503 (Service Unavailable) reply.

## Host Memory

The service should not die, however, it will take some time until
*x0* and all its plugins properly handle host memory excaustion.

## File Descriptors

Same as with host memory, but easier handleble.

# Backend Failure and Recovery

If a backend fails surving a request to the client, meaning, no data has
yet been sent to the client, the backend should not just be marked as *Offline*
but also, the request should directly be requeued to the next available
backend and retried up to M times with a timeout of N seconds.

If either limit has been reached without success, the client is to
receive a respective 5xx error response.

## Backend Health Checking

### Mode 1 (Lazy)

Backends are by default assumed to be in state *Online*.

If a backend fails to serve a request, e.g. due to networking or protocol
errors, the backend gets marked as *Offline*, and is in turn suspect to
health checks every N seconds, as defined per backend.

That means, health checks are only performed on *offline* backends, to
detect, when they've become online, again.
The health checks are disabled when the backend has been marked as *online*
and is in enabled-state.

If N successfull health checks pass, the backend
becomes *Online* again, and can receive new requests from this time on.

### Mode 2 (Opportunistic)

Additionally to mode 1, the backends get checked for healthiness when
being idle for at least N seconds.

Performing a health check while being in *Online* mode charges one load unit,
so if the backend allows 3 concurrent requests, only 2 are left for real
requests.

### Mode 3 (Paranoid)

Health checks are always performed, regardless of the backend's state
and activity.

## Sticky Offline Mode

There is a need for backends to stay out of the cluster, once being *offline*,
and must be enabled explicitely by the administrator (or by a script),
once it got ensured, that the code on that node is up-to-date, to not deliver
out-of-date content.

This sticky-mode not enabled by default.

# Dynamic Directors

While it is possible to define a static set of backends, one may also
create a dynamic director, that is, its backends are managed dynamically
via the _Director API_ and will be stored in a plain-text file in /var/lib/x0/
to preserve state over process restarts.

# Director API

The *director API* is provided over HTTP, to allow inspecting the live state
of each director of x0.

Static directors can be inspected, and enabled/disabled at runtime.
But dynamic directors can be fully configured at runtime,
i.g adding new backends, removing or updating existing ones.

# Implementation Details

## Thread Safety

The core x0 API ensures, that every I/O activity, caused by its HTTP connection,
is always processed within the same worker thread, while listener sockets
always accept new connections on the first (main) worker and then schedule
them to the least active worker.

We assume, that I/O activity from origin servers are to be processed
within the same worker thread as the frontend connection.

Health check timers on the other hand do not have any frontend connection
associated and thus, are assigned to least active worker at the time
the timer gets spawned.

# Ideas

## Dynamic `pass`-handler:

`director.create` is passed an identifier, that is used to create a new
handler `%s.pass` (or `director.%s.pass`) with the director instance as context,

    import director;
    
    handler setup {
        listen 80;
        director.create 'app123'
            'node01' => 'http://10.10.40.3:8080/?capacity=4',
            'node02' => 'http://10.10.40.4:8080/?capacity=4'
    }
    
    handler main {
        app1.pass if req.path =^ '/special/'
    }

# Design

# Example Configuration

    import director;

    handler setup {
        director.create 'app-cluster',
            'app01' => 'http://10.10.40.3:8080/?capacity=4',
            'app02' => 'http://10.10.40.4:8080/?capacity=4',
            'app03' => 'http://10.10.40.5:8080/?capacity=4',
            'app04' => 'http://10.10.40.6:8080/base/?capacity=8&disable',
            'app05' => 'http://10.10.40.7:8080/?capacity=4&backup',
            'app06' => 'fastcgi://10.10.40.8:8080/?capacity=4&backup',
            'app07' => 'fastcgi:///var/run/app.sock?capacity=4&backup',

        listen 80;
    }

    handler main {
        director.api '/x0/director'
        director.pass 'app-cluster';
    }

# Admin JSON API

By default, this API is not self-protected, but you can easily
configure with with basic-auth.

    handler main {
        if req.path =^ '/x0' {
            auth.realm "http admin area"
            auth.userfile "/etc/htpasswd"
            auth.require

            director.api '/x0/director'
            status.api '/x0/status'
            errorlog.api '/x0/errorlog'
        }
    }


    `GET /x0/director/`
    ->  { "cluster_1": {
            "total": NUMBER,
            "members": [
              {"name": NAME, "total": NUMBER, "load": NUMBER, "capacity": NUMBER}
            ]
          },
          ...
        }

    `POST $PREFIX/directors/app123?backend=node01&action=enable`

    `POST $PREFIX/directors/$DIRECTOR?backend=$BACKEND&action=$ACTION`
    with $ACTION one of enable, disable

    `GET $PREFIX/event-stream`
    sends out state change notifications in SSE-manner (server-sent events)

# Implementation

New class naming proposal:

    class DirectorPlugin;
    class Director;
    class RequestNotes;
    class HealthMonitor;
    class Backend;
        class HttpBackend;
        class FastcgiBackend;
        class NullBackend;

