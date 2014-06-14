/* <HttpRequest.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef x0_http_request_h
#define x0_http_request_h (1)

#include <x0/http/HttpHeader.h>
#include <x0/http/HttpConnection.h>
#include <x0/http/HttpWorker.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpStatus.h>
#include <x0/http/HttpFileRef.h>
#include <x0/io/Source.h>
#include <x0/io/CompositeSource.h>
#include <x0/io/CallbackSource.h>
#include <x0/CustomDataMgr.h>
#include <x0/RegExp.h>
#include <x0/Logging.h>
#include <x0/Severity.h>
#include <x0/Buffer.h>
#include <x0/Signal.h>
#include <x0/strutils.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <string>
#include <vector>
#include <list>
#include <utility>
#include <memory>
#include <cassert>

namespace x0 {

class HttpPlugin;
class HttpConnection;

//! \addtogroup http
//@{

/**
 * \brief a client HTTP reuqest object, holding the parsed x0 request data.
 *
 * \see header, response, HttpConnection, server
 */
class X0_API HttpRequest :
#ifndef XZERO_NDEBUG
    public Logging,
#endif
    public RegExpContext
{
    CUSTOMDATA_API_INLINE

public:
    class HeaderList // {{{
    {
    public:
        struct Header { // {{{
            std::string name;
            std::string value;
            Header *prev;
            Header *next;

            Header(const std::string& _name, const std::string& _value, Header *_prev, Header *_next) :
                name(_name), value(_value), prev(_prev), next(_next)
            {
                link(prev, next);
            }

            Header(const BufferRef& _name, const BufferRef& _value, Header *_prev, Header *_next) :
                name(_name.str()), value(_value.str()), prev(_prev), next(_next)
            {
                link(prev, next);
            }

            Header(const char* _name, const char* _value, Header *_prev, Header *_next) :
                name(_name), value(_value), prev(_prev), next(_next)
            {
                link(prev, next);
            }

        private:
            void link(Header* prev, Header* next)
            {
                if (prev) {
                    prev->next = this;
                }

                if (next) {
                    next->prev = this;
                }
            }

        };
        // }}}

        class iterator { // {{{
        private:
            Header *current_;

        public:
            iterator() :
                current_(NULL)
            {}

            explicit iterator(Header *item) :
                current_(item)
            {}

            Header& operator*()
            {
                return *current_;
            }

            Header& operator->()
            {
                return *current_;
            }

            Header *get() const
            {
                return current_;
            }

            iterator& operator++()
            {
                if (current_)
                    current_ = current_->next;

                return *this;
            }

            friend bool operator==(const iterator& a, const iterator& b)
            {
                return &a == &b || a.current_ == b.current_;
            }

            friend bool operator!=(const iterator& a, const iterator& b)
            {
                return !operator==(a, b);
            }
        };
        // }}}

    private:
        size_t size_;
        Header *first_;
        Header *last_;

    public:
        HeaderList() :
            size_(0), first_(NULL), last_(NULL)
        {}

        ~HeaderList()
        {
            clear();
        }

        void clear()
        {
            while (first_) {
                delete unlinkHeader(first_);
            }
        }

        iterator begin() { return iterator(first_); }
        iterator end() { return iterator(); }

        std::size_t size() const
        {
            return size_;
        }

        bool contains(const std::string& name) const
        {
            return const_cast<HeaderList*>(this)->findHeader(name) != nullptr;
        }

        template<typename Key, typename Value>
        void push_back(const Key& name, const Value& value)
        {
            last_ = new Header(name, value, last_, NULL);

            if (first_ == NULL)
                first_ = last_;

            ++size_;
        }

        Header *findHeader(const std::string& name)
        {
            Header *item = first_;

            while (item != NULL)
            {
                if (iequals(item->name.c_str(), name.c_str()))
                    return item;

                item = item->next;
            }

            return NULL;
        }

        void overwrite(const std::string& name, const std::string& value)
        {
            if (Header *item = findHeader(name))
                item->value = value;
            else
                push_back(name, value);
        }

        const std::string& operator[](const std::string& name) const
        {
            Header *item = const_cast<HeaderList *>(this)->findHeader(name);
            if (item)
                return item->value;

            static std::string not_found;
            return not_found;
        }

        std::string& operator[](const std::string& name)
        {
            Header *item = findHeader(name);
            if (item)
                return item->value;

            static std::string not_found;
            return not_found;
        }

        void append(const std::string& name, const std::string& value)
        {
            // TODO append value to the header with name or create one if not yet available.
        }

        Header *unlinkHeader(Header *item)
        {
            Header *prev = item->prev;
            Header *next = item->next;

            // unlink from list
            if (prev)
                prev->next = next;

            if (next)
                next->prev = prev;

            // adjust first/last entry points
            if (item == first_)
                first_ = first_->next;

            if (item == last_)
                last_ = last_->prev;

            --size_;

            return item;
        }

        void remove(const std::string& name)
        {
            if (Header *item = findHeader(name))
            {
                delete unlinkHeader(item);
            }
        }
    }; // }}}

private:
    /// pre-computed string representations of status codes, ready to be used by serializer
    static char statusCodes_[512][4];

public:
    explicit HttpRequest(HttpConnection& connection);
    ~HttpRequest();

    void clear();

    Signal<void()> onPostProcess;
    Signal<void()> onRequestDone;

    HttpConnection& connection;					///< the TCP/IP connection this request has been sent through

    // request properties
    BufferRef method;							///< HTTP request method, e.g. HEAD, GET, POST, PUT, etc.
    BufferRef unparsedUri;					///< unparsed request URI
    Buffer path;								///< URL-decoded path-part
    BufferRef query;							///< URL-encoded query string
    BufferRef pathinfo;						///< PATH_INFO part of the HTTP request path.
    HttpFileRef fileinfo;						///< the final entity to be served, for example the full path to the file on disk.
    int httpVersionMajor;						///< HTTP protocol version major part that this request was formed in
    int httpVersionMinor;						///< HTTP protocol version minor part that this request was formed in
    BufferRef hostname;						///< Host header field.
    std::vector<HttpRequestHeader> requestHeaders; ///< request headers
    unsigned long long bytesTransmitted_;

    /** retrieve value of a given request header */
    BufferRef requestHeader(const BufferRef& name) const;
    std::string requestHeaderCumulative(const std::string& name) const;

    void removeRequestHeaders(const std::initializer_list<BufferRef>& names);

    std::string cookie(const std::string& name) const;

    void updatePathInfo();

    // accumulated request data
    std::string username;						///< username this client has authenticated with.
    Buffer documentRoot;                        ///< the document root directory for this request.

//	std::string if_modified_since;				//!< "If-Modified-Since" request header value, if specified.
//	std::shared_ptr<range_def> range;			//!< parsed "Range" request header
    bool expectingContinue;

public:
    void setErrorHandler(const std::function<bool(HttpRequest*)>& cb){ errorHandler_ = cb; }

    template<typename T, void (T::*cb)(Buffer& output)> void registerInspectHandler(T* object);
    void inspect(Buffer& output);

    // utility methods
    bool supportsProtocol(int major, int minor) const;
    std::string hostid() const;
    void setHostid(const std::string& custom);

    // content management
    bool contentAvailable() const { return body_ && body_->size() > 0; }
    std::unique_ptr<Source>&& takeBody() { return std::move(body_); }
    std::unique_ptr<Source>& body() { return body_; }

    template<typename... Args>
    void log(Severity s, Args&&... args);

    void log(LogMessage&& msg);

    // response
    HttpStatus status;           //!< HTTP response status code.
    HeaderList responseHeaders; //!< the headers to be included in the response.
    ChainFilter outputFilters;  //!< response content filters
    bool isResponseContentForbidden() const;

    unsigned long long bytesTransmitted() const;

    // full response writer - but do not invoke finish()
    bool sendfile();
    bool sendfile(const std::string& filename);
    bool sendfile(const HttpFileRef& transferFile);

    // dynamic response writer
    void write(std::unique_ptr<Source>&& chunk);
    template<class T, class... Args> void write(Args&&... args);

    template<class K, void (K::*Callback)()> bool writeCallback(K* object);
    bool writeCallback(CallbackSource::Callback cb);

    void setAbortHandler(std::function<void()>&& cb);
    void finish();
    bool isFinished() const { return connection.state() == HttpConnection::SendingReplyDone; }

    /**
     * Abnormally aborts given request and immediately closes the underlying connection.
     */
    void abort() { connection.abort(); }

    static std::string statusStr(HttpStatus status);

    template<typename T> inline void post(T function) { connection.post(function); }

    // security advisory
    bool testDirectoryTraversal();

    DateTime timeStart() const { return timeStart_; }
    TimeSpan duration() const { return connection.worker().now() - timeStart_; }

private:
    std::list<std::pair<void*, void (*)(void*, Buffer&)> > inspectHandlers_;

    mutable std::string hostid_;
    int directoryDepth_;

    std::function<bool(HttpRequest*)> errorHandler_;

    bool setUri(const BufferRef& uri);

    template<typename T, void (T::*cb)(Buffer&)>
    static void inspect_thunk(void* object, Buffer& output);

    template<class K, void (K::*cb)()>
    static void write_cb_thunk(void* data);

    HttpStatus verifyClientCache(const HttpFileRef& transferFile) const;
    bool processRangeRequest(const HttpFileRef& transferFile, int fd);

    // response write helper
    std::unique_ptr<Source> serialize();
    void writeDefaultResponseContent();
    void finalize();

    static void initialize();

    friend class HttpServer;
    friend class HttpConnection;

private:
    DateTime timeStart_;
    std::unique_ptr<Source> body_;
};

// {{{ request impl
inline void HttpRequest::clear()
{
    onPostProcess.clear();
    onRequestDone.clear();
    method.clear();
    unparsedUri.clear();
    path.clear();
    query.clear();
    pathinfo.clear();
    fileinfo.reset();
    httpVersionMajor = httpVersionMinor = 0;
    hostname.clear();
    requestHeaders.clear();
    bytesTransmitted_ = 0;
    username.clear();
    documentRoot.clear();
    expectingContinue = false;
    status = HttpStatus::Undefined;
    responseHeaders.clear();
    outputFilters.clear();
    inspectHandlers_.clear();
    hostid_.clear();
    directoryDepth_ = 0;
    errorHandler_ = std::function<bool(HttpRequest*)>();;

    body_.reset(nullptr);
}

inline bool HttpRequest::supportsProtocol(int major, int minor) const
{
    if (major == httpVersionMajor && minor <= httpVersionMinor)
        return true;

    if (major < httpVersionMajor)
        return true;

    return false;
}

template<class K, void (K::*cb)()>
void HttpRequest::write_cb_thunk(void* data)
{
    (static_cast<K*>(data)->*cb)();
}

template<typename... Args>
inline void HttpRequest::log(Severity s, Args&&... args)
{
    if (s >= connection.worker().server().logLevel()) {
        connection.log(s, args...);
    }
}

inline void HttpRequest::log(LogMessage&& msg)
{
    if (msg.severity() >= connection.worker().server().logLevel()) {
        connection.log(std::move(msg));
    }
}

template<class K, void (K::*cb)()>
bool HttpRequest::writeCallback(K* object)
{
    return writeCallback([=]() {
        (object->*cb)();
    });
}

/*! writes given data to the underlying connection.
 *
 * \param T type of chunk to write. must be derived from \ref Source.
 * \param args a list of arguments being passed to the source chunk.
 *
 * \code
 *   request->write<BufferRefSource>("Hello, World\r\n");
 *   request->write<FileSource>("/var/www/notes.html");
 * \endcode
 *
 * \see BufferSource, CallbackSource, CompositeSource, FileSource, FilterSource, Source
 */
template<class T, class... Args>
inline void HttpRequest::write(Args&&... args)
{
    write(std::unique_ptr<T>(new T(std::move(args)...)));
}

/*! retrieves the number of bytes successfully transmitted within this very request.
 *
 * \note this includes protocol-dependant bytes, too, such as transfer encoding and HTTP response headers.
 */
inline unsigned long long HttpRequest::bytesTransmitted() const
{
    return bytesTransmitted_;
}

/*! Checks wether given code MUST NOT have a response body.
 */
inline bool HttpRequest::isResponseContentForbidden() const
{
    return x0::content_forbidden(status);
}

template<typename T, void (T::*cb)(Buffer& output)>
inline void HttpRequest::registerInspectHandler(T* object)
{
    inspectHandlers_.push_back(std::make_pair(object, &inspect_thunk<T, cb>));
}

template<typename T, void (T::*cb)(Buffer&)>
void HttpRequest::inspect_thunk(void* object, Buffer& output)
{
    (static_cast<T*>(object)->*cb)(output);
}

inline void HttpRequest::inspect(Buffer& output)
{
    for (auto& handler: inspectHandlers_) {
        handler.second(handler.first, output);
    }
}
// }}}
//@}

} // namespace x0

#endif
