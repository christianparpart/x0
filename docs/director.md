# director - x0 Load Balancing Plugin

# Requirements

- should support multiple backend protocols
  - HTTP (via TCP and UNIX domain sockets)
  - FastCGI (via TCP and UNIX domain sockets)
  - ideally costom Flow handlers
- support request retries and per-director retry-limits
- support (per director) connect/read/write timeouts to backend
- provide admin REST-API for browsing, controlling directors, its backends, and their states/stats.
- reuse already existing code as used in `proxy` and `fastcgi` plugins.
  - existing plugins (proxy, fastcgi) should now reuse the new core API

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
        director.admin 'app-cluster' if request.path == '/director?admin'
        director.pass 'app-cluster';
    }

