/* <HttpRequest.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
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
			for (const Header *i = first_; i != NULL; i = i->next)
				if (strcasecmp(i->name.c_str(), name.c_str()) == 0)
					return true;

			return false;
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
				if (strcasecmp(item->name.c_str(), name.c_str()) == 0)
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

	// state whether response headers have been already sent or not.
	bool headersSent_;

public:
	explicit HttpRequest(HttpConnection& connection);
	~HttpRequest();

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

	/** retrieve value of a given request header */
	BufferRef requestHeader(const std::string& name) const;

	void updatePathInfo();

	// accumulated request data
	BufferRef username;							///< username this client has authenticated with.
	std::string documentRoot;					///< the document root directory for this request.

//	std::string if_modified_since;				//!< "If-Modified-Since" request header value, if specified.
//	std::shared_ptr<range_def> range;			//!< parsed "Range" request header
	bool expectingContinue;

	// custom data bindings
	//std::map<HttpPlugin *, CustomDataPtr> customData;

	// utility methods
	bool supportsProtocol(int major, int minor) const;
	std::string hostid() const;
	void setHostid(const std::string& custom);

	// content management
	bool contentAvailable() const;
	bool read(const std::function<void(BufferRef&&)>& callback);

	template<typename... Args>
	void log(Severity s, Args&&... args);

	// response
	HttpError status;           //!< HTTP response status code.
	HeaderList responseHeaders; //!< the headers to be included in the response.
	ChainFilter outputFilters;  //!< response content filters
	bool headersSent() const;   //!< returns true in case serializing the response has already been started, that is, headers has been sent out already.
	bool isResponseContentForbidden() const;
	void write(const SourcePtr& source, const CompletionHandlerType& handler);
	void setClientAbortHandler(void (*callback)(void *), void *data = NULL);
	void finish();

	static std::string statusStr(HttpError status);

private:
	mutable std::string hostid_;
	std::function<void(BufferRef&&)> readCallback_;

	void onRequestContent(BufferRef&& chunk);

	// response write helper
	void onWriteHeadersComplete(int ec, const SourcePtr& content, const CompletionHandlerType& handler);
	void writeContent(const SourcePtr& content, const CompletionHandlerType& handler);
	SourcePtr serialize();
	SourcePtr makeDefaultResponseContent();
	void onFinished(int ec);

	static void initialize();

	friend class HttpServer;
	friend class HttpConnection;
};

// {{{ request impl
inline bool HttpRequest::supportsProtocol(int major, int minor) const
{
	if (major == httpVersionMajor && minor <= httpVersionMinor)
		return true;

	if (major < httpVersionMajor)
		return true;

	return false;
}

template<typename... Args>
inline void HttpRequest::log(Severity s, Args&&... args)
{
	connection.worker().server().log(s, args...);
}

inline bool HttpRequest::headersSent() const
{
	return headersSent_;
}

/** write given source to response content and invoke the completion handler when done.
 *
 * \param content the content (chunk) to push to the client
 * \param handler completion handler to invoke when source has been fully flushed or if an error occured
 *
 * \note this implicitely flushes the response-headers if not yet done, thus, making it impossible to modify them after this write.
 */
inline void HttpRequest::write(const SourcePtr& content, const CompletionHandlerType& handler)
{
	if (headersSent_)
		writeContent(content, handler);
	else
		connection.writeAsync(serialize(), 
			std::bind(&HttpRequest::onWriteHeadersComplete, this, std::placeholders::_1, content, handler));
}

/** is invoked as completion handler when sending response headers. */
inline void HttpRequest::onWriteHeadersComplete(int ec, const SourcePtr& content, const CompletionHandlerType& handler)
{
	headersSent_ = true;

	if (!ec)
	{
		// write response content
		writeContent(content, handler);
	}
	else
	{
		// an error occured -> notify completion handler about the error
		handler(ec, 0);
	}
}

inline void HttpRequest::writeContent(const SourcePtr& content, const CompletionHandlerType& handler)
{
	if (outputFilters.empty())
		connection.writeAsync(content, handler);
	else
		connection.writeAsync(std::make_shared<FilterSource>(content, outputFilters, false), handler);
}

/** checks wether given code MUST NOT have a response body. */
inline bool HttpRequest::isResponseContentForbidden() const
{
	return x0::content_forbidden(status);
}
// }}}

//@}

} // namespace x0

#endif
