/* <HttpRequest.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef x0_http_request_h
#define x0_http_request_h (1)

#include <x0/http/HttpHeader.h>
#include <x0/http/HttpConnection.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpError.h>
#include <x0/io/FilterSource.h>
#include <x0/io/CallbackSource.h>
#include <x0/io/FileInfo.h>
#include <x0/CustomDataMgr.h>
#include <x0/Logging.h>
#include <x0/Severity.h>
#include <x0/Buffer.h>
#include <x0/BufferRef.h>
#include <x0/strutils.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <string>
#include <vector>
#include <list>
#include <utility>
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
struct X0_API HttpRequest :
#ifndef NDEBUG
	public Logging,
#endif
	public CustomDataMgr
{
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
				if (prev)
					prev->next = this;

				if (next)
					next->prev = this;
			}

			~Header()
			{
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

		void push_back(const std::string& name, const std::string& value)
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

	// response state
	enum OutputState {
		//! nothing has been sent (or triggered for sending) to the client yet.
		Unhandled,
		//! at least response headers are to be written to the client.
		Populating,
		//! response message is fully populated
		Finished
	};

	OutputState outputState_;

public:
	explicit HttpRequest(HttpConnection& connection);
	~HttpRequest();

	void clear();

	HttpConnection& connection;					///< the TCP/IP connection this request has been sent through

	// request properties
	BufferRef method;							///< HTTP request method, e.g. HEAD, GET, POST, PUT, etc.
	BufferRef uri;								///< parsed request uri
	BufferRef path;								///< decoded path-part
	FileInfoPtr fileinfo;						///< the final entity to be served, for example the full path to the file on disk.
	std::string pathinfo;
	BufferRef query;							///< decoded query-part
	int httpVersionMajor;						///< HTTP protocol version major part that this request was formed in
	int httpVersionMinor;						///< HTTP protocol version minor part that this request was formed in
	BufferRef hostname;							///< Host header field.
	std::vector<HttpRequestHeader> requestHeaders; ///< request headers
	unsigned long long bytesTransmitted_;

	/** retrieve value of a given request header */
	BufferRef requestHeader(const std::string& name) const;

	void updatePathInfo();

	// accumulated request data
	std::string username;						///< username this client has authenticated with.
	std::string documentRoot;					///< the document root directory for this request.

//	std::string if_modified_since;				//!< "If-Modified-Since" request header value, if specified.
//	std::shared_ptr<range_def> range;			//!< parsed "Range" request header
	bool expectingContinue;

public:
	void setErrorHandler(FlowValue::Function handler) { errorHandler_ = handler; }

	template<typename T, void (T::*cb)(Buffer& output)> void registerInspectHandler(T* object);
	void inspect(Buffer& output);

	// utility methods
	bool supportsProtocol(int major, int minor) const;
	std::string hostid() const;
	void setHostid(const std::string& custom);

	// content management
	bool contentAvailable() const;
	template<typename K, void (K::*cb)(const BufferRef&)> void setBodyCallback(K* object);
	void setBodyCallback(void (*callback)(const BufferRef&, void*), void* data = nullptr);

	template<typename... Args>
	void log(Severity s, Args&&... args);

	// response
	HttpError status;           //!< HTTP response status code.
	HeaderList responseHeaders; //!< the headers to be included in the response.
	ChainFilter outputFilters;  //!< response content filters
	bool isResponseContentForbidden() const;

	bool isAborted() const;

	OutputState outputState() const;
	unsigned long long bytesTransmitted() const;

	void write(Source* chunk);
	template<class K, void (K::*Callback)()>
	bool writeCallback(K* object);
	bool writeCallback(CallbackSource::Callback cb, void* data);
	template<class T, class... Args> void write(Args&&... args);

	void setAbortHandler(void (*callback)(void *), void *data = NULL);
	void finish();
	bool isFinished() const { return outputState_ == Finished; }

	static std::string statusStr(HttpError status);

private:
	std::list<std::pair<void*, void (*)(void*, Buffer&)> > inspectHandlers_;

	mutable std::string hostid_;

	void (*bodyCallback_)(const BufferRef&, void*);
	void* bodyCallbackData_;

	FlowValue::Function errorHandler_;

	void onRequestContent(const BufferRef& chunk);

	template<class K, void (K::*cb)(const BufferRef&)>
	static void body_cb_thunk(const BufferRef& chunk, void* data);

	template<typename T, void (T::*cb)(Buffer&)>
	static void inspect_thunk(void* object, Buffer& output);

	template<class K, void (K::*cb)()>
	static void write_cb_thunk(void* data);

	// response write helper
	Source* serialize();
	void writeDefaultResponseContent();
	void finalize();

	static void initialize();

	friend class HttpServer;
	friend class HttpConnection;
};

// {{{ request impl
inline void HttpRequest::clear()
{
	// custom data is to be cleared in finish() already. clear() is invoked in resume() to clear up the remaining properties and prepare for the next request
	// clearCustomData();

	outputState_ = Unhandled;

	method.clear();
	uri.clear();
	path.clear();
	fileinfo = FileInfoPtr();
	pathinfo = "";
	query.clear();
	httpVersionMajor = httpVersionMinor = 0;
	hostname.clear();
	requestHeaders.clear();
	bytesTransmitted_ = 0;

	username.clear();
	documentRoot = "";
	expectingContinue = false;

	hostid_ = ""; // hostname ':' port

	bodyCallback_ = nullptr;
	bodyCallbackData_ = nullptr;

	errorHandler_ = nullptr;

	status = HttpError::Undefined;
	responseHeaders.clear();
	outputFilters.clear();

	inspectHandlers_.clear();
}

inline bool HttpRequest::supportsProtocol(int major, int minor) const
{
	if (major == httpVersionMajor && minor <= httpVersionMinor)
		return true;

	if (major < httpVersionMajor)
		return true;

	return false;
}

template<typename K, void (K::*cb)(const BufferRef&)>
inline void HttpRequest::setBodyCallback(K* object)
{
	setBodyCallback(&body_cb_thunk<K, cb>, object);
}

template<class K, void (K::*cb)(const BufferRef&)>
void HttpRequest::body_cb_thunk(const BufferRef& chunk, void* data)
{
	(static_cast<K*>(data)->*cb)(chunk);
}

template<class K, void (K::*cb)()>
void HttpRequest::write_cb_thunk(void* data)
{
	(static_cast<K*>(data)->*cb)();
}

template<typename... Args>
inline void HttpRequest::log(Severity s, Args&&... args)
{
	connection.worker().server().log(s, args...);
}

/** write given source to response content and invoke the completion handler when done.
 *
 * \param chunk the content (chunk) to push to the client
 * \param handler completion handler to invoke when source has been fully flushed or if an error occured
 *
 * \note this implicitely flushes the response-headers if not yet done, thus, making it impossible to modify them after this write.
 */
inline void HttpRequest::write(Source* chunk)
{
	if (connection.isAborted()) {
		delete chunk;
		return;
	}

	switch (outputState_) {
		case Unhandled:
			outputState_ = Populating;
			connection.write(serialize());
			/* fall through */
		case Populating:
			if (outputFilters.empty())
				connection.write(chunk);
			else
				connection.write<FilterSource>(chunk, &outputFilters, false);
			break;
		case Finished:
			assert(0 && "BUG");
	}
}

template<class K, void (K::*Callback)()>
bool HttpRequest::writeCallback(K* object)
{
	return writeCallback(&write_cb_thunk<K, Callback>, object);
}

/*! appends a callback source into the output buffer if non-empty or invokes it directly otherwise.
 *
 * Invoke this method to get called back (notified) when all preceding content chunks have been
 * fully sent to the client already.
 *
 * This method either appends this callback into the output queue, thus, being invoked when all
 * preceding output chunks have been handled so far, or the callback gets invoked directly
 * when there is nothing in the output queue (meaning, that everything has been already fully
 * sent to the client).
 *
 * \retval true The callback will be invoked later (callback appended to output queue).
 * \retval false The output queue is empty (everything sent out so far *OR* the connection is aborted) and the callback was invoked directly.
 */
inline bool HttpRequest::writeCallback(CallbackSource::Callback cb, void* data)
{
	if (connection.isAborted()) {
		cb(data);
		return false;
	}

	assert(outputState_ == Populating);

	if (connection.isOutputPending()) {
		connection.write<CallbackSource>(cb, data);
		return true;
	} else {
		cb(data);
		return false;
	}
}

/*! writes given data to the underlying connection.
 *
 * \tparam T type of chunk to write. must be derived from \ref Source.
 * \param args a list of arguments being passed to the source chunk.
 *
 * \code
 *   request->write<BufferSource>("Hello, World\r\n");
 *   request->write<FileSource>("/var/www/notes.html");
 * \endcode
 *
 * \see BufferSource, CallbackSource, CompositeSource, EmptySource, FileSource, FilterSource, Source
 */
template<class T, class... Args>
inline void HttpRequest::write(Args&&... args)
{
	if (!isAborted()) {
		write(new T(std::move(args)...));
	}
}

inline HttpRequest::OutputState HttpRequest::outputState() const
{
	return outputState_;
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

/*! Tests whether or not the given request and underlying connection is already aborted or not.
 */
inline bool HttpRequest::isAborted() const
{
	return connection.isAborted();
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
