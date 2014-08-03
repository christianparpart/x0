
## Decoupling Attempt

The following other components need to be accessable ...

### HttpRequest

- current datetime: connection.worker().now()
- connection.state()
- connection abort: connection.abort()
- posting into the http i/o thread: connection.post()
- request duration: now() - timeStart()
- logging: connection.log()
- connection's remote|local ip|port
- access to thread-local HttpFileMgr in updatePathInfo()
- upon response serialization
  - onPostProcess hook: connection.worker.server.onPostProcess
  - connection.shouldKeepAlive
  - connection.worker.server.maxKeepAlive
  - connection.worker.server.maxKeepAliveRequests
  - connection.worker.server.advertise
  - connection.worker.server.tag
  - connection.worker.server.tcpCork
  - connection.socket.setTcpCork
- connection.isOpen
- connection.setState
- connection.write
- connection.isOutputPending
- connection.ref | unref
- inside finalize()
  - connection.worker.server.onRequestDone()
  - connection.clearRequestBody()
  - connection.resume()
  - connection.close()
- connection.clientAbortHandler
- connection.wantRead

28 deps

### HttpConnection

- worker.post() -- posting into the http i/o thread
- worker.server.requestHeaderBufferSize
- worker.server.requestBodyBufferSize
- worker.server.onConnectionClose()
- worker.release(HttpConnection self)
- worker.server.tcpNoDelay
- worker.server.lingering
- worker.server.onConnectionOpen()
- worker.server.maxReadIdle
- worker.server.maxWriteIdle
- worker.server.maxKeepAlive
- worker.now()
- worker.server.maxRequestUriSize
- worker.server.maxRequestHeaderSize
- worker.server.maxRequestHeaderCount
- worker.server.maxRequestBodySize
- worker.server.requestHeaderBufferSize
- worker.server.requestBodyBufferSize
- worker.handleRequest()
- worker.server.onRequestDone()
- worker.server.onConnectionStateChanged()
- worker.log()

21 deps

### HttpWorker

- server.currentWorker()
- server.onWorkerSpawn()
- server.onWorkerUnspawn()
- server.log()
- in handleRequest:
  - server.onPreProcess()
  - server.requestHandler()

6 deps

## re-API'ing


```cpp

HttpRequest(
    DateTime timeStart,
    )

HttpResponse(
    unsigned maxKeepAliveRequests,
    bool advertise,
    string serverTag,
    bool tcpCork,
    Socket* socket,
    RequestHook onPostProcess,
    RequestHook onRequestDone,
    )
  void abort();
  void finish();
  void write();

HttpConnection(
    // callbacks
    ConnectionHook onClientAbort,
    ConnectionHook onConnectionOpen,
    ConnectionHook onConnectionClose,
    ConnectionHook onRequestDone,
    ConnectionHook onConnectionStateChanged,
    // performance tweaks
    bool tcpCork,
    // resource limits
    TimeSpan maxReadIdle,
    TimeSpan maxWriteIdle,
    TimeSpan maxKeepaliveIdle,
    TimeSpan lingering,
    size_t maxRequestUriSize,
    size_t maxRequestHeaderSize,
    size_t maxRequestHeaderCount,
    size_t maxRequestBodySize,
    size_t requestHeaderBufferSize,
    size_t requestBodyBufferSize,
    // other
    Logger* logger,
    Clock* clock,
    )

HttpWorker

HttpServer

```

```cpp
// some helper interfaces for the above redesign
class Clock {
 public:
  virtual time_t now() const noexept = 0;
};

// specializations as used by xzero / x0
class EVClock : public Clock {
 public:
  explicit EVClock(ev::loop* loop);

  time_t now() const noexept override { return ev_now(loop); }
};

/**
 * @brief abstrats a task queue.
 *
 * tasks get enqueued on this end, and will be executed on other receiver end. 
 */
class TaskQueue {
 public:
  typedef std::function<void()> Task;
  typedef std::function<void(Task)> TaskAcceptor;

  explicit TaskQueue(TaskAcceptor onNewTask);

  /**
   * @brief To enqueue one proc to be run within this thread.
   * @note can be invoked from any thread
   */
  void enqueue(Task&& task);
};

```
