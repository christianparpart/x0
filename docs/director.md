# director - x0 Load Balancing Plugin

# Requirements

- support multiple backend protocols
  - HTTP (via TCP and UNIX domain sockets)
  - FastCGI (via TCP and UNIX domain sockets)
- active/standby/backup backend modes, where standby backends get only used when all active
  backends are at its capacity limits (and/or offline/disabled).
- request queue with a per-director limit, used when no active nor standby backend can currently process 
- [TODO] support (per director) connect/read/write timeouts to backend
- support retrying requests and per-director retry-limits
- sticky offline mode
  (when a backend becomes offline, it gets disabled, too,
  so that it won't be used again, once being back online)
- support dynamic directors
  - adding/removing/editing backends at runtime
  - storing backend configuration in some plain file in /var/lib/x0/director/$NAME.db
- client side routing
- provide admin JSON-API for browsing, controlling directors, its backends, and their states/stats.

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

## Retrieving State of all Directors

    curl -v http://localhost:8080/x0/director/

This will return a big *application/json* response containing
a list of all directors and their configuration and state.

## Retrieving single Director State

    curl -v http://localhost:8080/x0/director/app_cluster

Retrieves state of only one director, by name.

## Updating a Director

    curl -v http://localhost:8080/x0/director/app_cluster -X POST \
        -d health-check-host-header=example.com \
        -d health-check-request-path=/health \
        -d health-check-fcgi-script-filename= \
        -d queue-limit=128 \
        -d queue-timeout=10000 \
        -d retry-after=60 \
        -d max-retry-count=3 \
        -d sticky-offline-mode=false

You can reconfigure any of the directors parameters
unless this director has been created statically.

### Director Properties

- *health-check-host-header*: defines the Host request-header to be passed when performing health-check requests
- *health-check-request-path*: the request-path to be passed when performing health-check requests
- *health-check-fcgi-script-filename*: this is a special property and only applies to FastCGI backends that
  require a physical underlying file to actually serve the request (such as PHP). This value will
  translate into the CGI environment variable called *SCRIPT_FILENAME*.
- *queue-limit*: maximum number of requests that may be enqueued at the same time.
  Exceeding this value results into 503 (Service Unavailable) instead of enqueuing further.
  This mechanism is to reduce resource excess.
- *queue-timeout*: The time-span in milliseconds how long a request may reside in the queue.
  Exceeding this value will result into a 503 (Service Unavailable).
- *retry-after*: This value will give the client the delta in seconds when to retry 
  the failing request.
  While all time-span values are usually given in milliseconds, this value is not, because
  the HTTP protocol states, that the corresponding response header is given in seconds.
- *max-retry-count*: The number of attempts a request is being tried to be passed to one of the
  existing backends. If a backend has been chosen and the request being passed, but the backend
  fails connecting to its origin server, the request will be retried up to N times.
  Exceeding this value will result into a 503 (Service Unavailable).
- *sticky-offline-mode*: When a backend becomes unavailable (offline) due to failures, then no further
  attempts are made to deliver any further requests until its corresponding health monitor has
  detected this backend to be online again.
  Now, if this property is set to false, the backend may be included into the set of available backends again,
  but if set to false, it will still become "Online" but also auto-disabled by the health monitor
  to avoid serving requests by a backend that potentially has been to long offline, that it 
  might have old backend logig. So setting this value to true might be a wise consideration
  in continuous developing production environments.

## Retrieving Backend State

    curl -v http://localhost:8080/x0/director/app_cluster/backend01

Retrieves state of only one backend from a director, by names.

## Creating a new Backend

    curl -v http://localhost:8080/x0/director/app_cluster -X PUT \
        -d role=active \
        -d enabled=false \
        -d capacity=2 \
        -d health-check-mode=paranoid \
        -d health-check-interval=2 \
        -d protocol=http \
        -d hostname=127.0.0.1 \
        -d port=3101 \
        -d name=backend01

The *name*-parameter can be also specified as part of the URI path, which
will always be preferred via request body of *name*.

Others must be specified via request body.

## Updating an existing Backend

    curl -v http://localhost:8080/x0/director/app_cluster/backend01 -X POST \
        -d role=active \
        -d enabled=false \
        -d capacity=2 \
        -d health-check-mode=paranoid \
        -d health-check-interval=2

You cannot edit all attributes, but:

- role
- enabled
- capacity
- health-check-mode
- health-check-interval

Updating name, protocol, hostname, or port would literally be the same as just creating a new backend.

## Deleting a Backend

    curl -v http://localhost:8080/x0/director/app_cluster/backend01 -X DELETE

The wait parameter specifies whether or not to kill existing connections on
given backend or if the backend should wait for them to finish before getting
removed completely out of the cluster.

Deleted but not yet removed backends change their state to "*Terminating*".

# Plugin Improvement Ideas

## Request Delivery Timings

Provide the ability to graph the average wait time of requests from the
point the plugin receied the request until they where actually accepted
by some backend to be processed.
This data might be interesting for very active clusters.

## Very nice Web UI atop the JSON API

[Paul Asmuth](http://github.com/paulasmuth) is working on this one. Thanks man. :-)

