/* <HttpConnection.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef x0_connection_h
#define x0_connection_h (1)

#include <x0/http/HttpMessageParser.h>
#include <x0/http/HttpStatus.h>
#include <x0/io/CompositeSource.h>
#include <x0/io/SocketSink.h>
#include <x0/CustomDataMgr.h>
#include <x0/LogMessage.h>
#include <x0/TimeSpan.h>
#include <x0/Socket.h>
#include <x0/Buffer.h>
#include <x0/Property.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <x0/sysconfig.h>

#include <functional>
#include <memory>
#include <string>
#include <ev++.h>

namespace x0 {

//! \addtogroup http
//@{

class HttpRequest;
class HttpWorker;
class ServerSocket;

/**
 * @brief HTTP client connection object.
 * @see HttpRequest, HttpServer
 */
class X0_API HttpConnection :
    public HttpMessageParser
{
    CUSTOMDATA_API_INLINE

public:
    enum State {
        Undefined = 0,			//!< Object got just constructed.
        ReadingRequest,			//!< Parses HTTP request.
        ProcessingRequest,		//!< request handler: has taken over but not sent out anythng
        SendingReply,			//!< request handler: response headers written, sending body
        SendingReplyDone,		//!< request handler: populating message done, still pending data to sent.
        KeepAliveRead			//!< Waiting for next HTTP request in keep-alive state.
    };

    class ScopedRef {
    public:
        ScopedRef& operator=(const ScopedRef&) = delete;
        ScopedRef(const ScopedRef&) = delete;

        explicit ScopedRef(HttpConnection* c) :
            c_(c)
        {
            c_->ref();
        }

        ~ScopedRef()
        {
            c_->unref();
        }

    private:
        HttpConnection* c_;
    };

public:
    HttpConnection& operator=(const HttpConnection&) = delete;
    HttpConnection(const HttpConnection&) = delete;

    /**
     * creates an HTTP connection object.
     * \param srv a ptr to the server object this connection belongs to.
     */
    HttpConnection(HttpWorker* worker, unsigned long long id);

    ~HttpConnection();

    unsigned long long id() const;				//!< returns the (mostly) unique, worker-local, ID to this connection

    unsigned requestCount() const { return requestCount_; }

    State state() const { return state_; }
    void setState(State value);
    const char* state_str() const;

    HttpMessageParser::State parserState() const { return HttpMessageParser::state(); }
    const char* parserStateStr() const { return HttpMessageParser::state_str(); }

    Socket* socket() const;						//!< Retrieves a pointer to the connection socket.
    HttpWorker& worker() const;					//!< Retrieves a reference to the owning worker.

    const IPAddress& remoteIP() const { return socket_->remoteIP(); }
    unsigned int remotePort() const { return socket_->remotePort(); } //!< Retrieves the TCP port numer of the remote end point (client).

    const IPAddress& localIP() const { return socket_->localIP(); }
    unsigned int localPort() const { return socket_->localPort(); }

    const ServerSocket& listener() const;

    bool isSecure() const;

    void write(std::unique_ptr<Source>&& source);
    template<class T, class... Args> void write(Args&&... args);

    void flush();
    bool autoFlush() const { return autoFlush_; }
    void setAutoFlush(bool value) { autoFlush_ = value; if (value) { flush(); } }

    bool isOutputPending() const { return !output_.empty(); }

    const HttpRequest* request() const { return request_; }
    HttpRequest* request() { return request_; }

    bool isInputPending() const { return requestParserOffset_ < requestBuffer_.size(); }

    unsigned refCount() const;

    void post(std::function<void()> function);

    bool isOpen() const;

    template<typename... Args>
    void log(Severity s, const char* fmt, Args... args);

    void log(LogMessage&& msg);

    /** Increments the internal reference count and ensures that this object remains valid until its unref().
     *
     * Surround the section using this object by a ref() and unref(), ensuring, that this
     * object won't be destroyed in between.
     *
     * \see unref()
     * \see close()
     * \see HttpRequest::finish()
     */
    void ref();

    /** Decrements the internal reference count, marking the end of the section using this connection.
     *
     * \note After the unref()-call, the connection object MUST NOT be used any more.
     * If the unref()-call results into a reference-count of zero <b>AND</b> the connection
     * has been closed during this time, the connection will be released / destructed.
     *
     * \see ref()
     */
    void unref();

private:
    friend class HttpRequest;
    friend class HttpWorker;

    void clearRequestBody();

    void reinit(unsigned long long id);
    void start(std::unique_ptr<Socket>&& client, ServerSocket* listener);
    void resume();

    void abort(HttpStatus status);
    void abort();
    void close();

    void onHandshakeComplete(Socket*);
    bool readSome();
    bool writeSome();
    bool process();
    void onReadWriteReady(Socket* socket, int revents);
    void onReadWriteTimeout(Socket* socket);

    void wantRead(const TimeSpan& timeout);
    void wantWrite();

    // overrides from HttpMessageParser:
    bool onMessageBegin(const BufferRef& method, const BufferRef& entity, int versionMajor, int versionMinor) override;
    bool onMessageHeader(const BufferRef& name, const BufferRef& value) override;
    bool onMessageHeaderEnd() override;
    bool onMessageContent(const BufferRef& chunk) override;
    bool onMessageEnd() override;
    void onProtocolError(const BufferRef& chunk, size_t offset) override;

    void setShouldKeepAlive(bool enabled);
    bool shouldKeepAlive() const { return shouldKeepAlive_; }

public:
    const Buffer& requestBuffer() const { return requestBuffer_; }
    std::size_t requestParserOffset() const { return requestParserOffset_; }

private:
    unsigned refCount_;

    State state_;

    ServerSocket* listener_;
    HttpWorker* worker_;

    unsigned long long id_;				        //!< the worker-local connection-ID
    unsigned requestCount_;				        //!< the number of requests already processed or currently in process
    bool shouldKeepAlive_;                      //!< indication whether or not connection should keep-alive after current request
    std::function<void()> clientAbortHandler_;  //!< connection abort callback

    // HTTP HttpRequest
    Buffer requestBuffer_;                      //!< buffer for incoming data.
    std::size_t requestParserOffset_;           //!< number of bytes in request buffer successfully processed already.
    std::size_t requestHeaderEndOffset_;        //!< offset to the first byte of the currently processed request
    HttpRequest* request_;				        //!< currently parsed http HttpRequest, may be NULL

    size_t requestBodyBufferSize_;              //!< number of bytes of the request body that is part of \p requestBuffer_.
    char requestBodyPath_[1024];                //!< full path to temporary stored request body, if available
    int requestBodyFd_;                         //!< file handle to temporary stored request body, if available
    size_t requestBodyFileSize_;                //!< size of the temporary request body file in bytes, if available, 0 otherwise.

    // output
    CompositeSource output_;			        //!< pending write-chunks
    std::unique_ptr<Socket> socket_;            //!< underlying communication socket
    SocketSink sink_;					        //!< sink wrapper for socket_
    bool autoFlush_;					        //!< true if flush() is invoked automatically after every write()

    // intrusive links for the free-list cache
    HttpConnection* prev_;
    HttpConnection* next_;
};

// {{{ inlines
inline Socket* HttpConnection::socket() const
{
    return socket_.get();
}

inline unsigned long long HttpConnection::id() const
{
    return id_;
}

inline unsigned HttpConnection::refCount() const
{
    return refCount_;
}

inline const char* HttpConnection::state_str() const
{
    static const char* str[] = {
        "undefined",
        "reading-request",
        "processing-request",
        "sending-reply",
        "sending-reply-done",
        "keep-alive-read"
    };
    return str[static_cast<size_t>(state_)];
}

inline HttpWorker& HttpConnection::worker() const
{
    return *worker_;
}

template<class T, class... Args>
inline void HttpConnection::write(Args&&... args)
{
    write(std::unique_ptr<T>(new T(args...)));
}

inline const ServerSocket& HttpConnection::listener() const
{
    return *listener_;
}

inline bool HttpConnection::isOpen() const
{
    return socket_->isOpen();
}

template<typename... Args>
inline void HttpConnection::log(Severity s, const char* fmt, Args... args)
{
    log(LogMessage(s, fmt, args...));
}
// }}}

//@}

} // namespace x0

#endif
