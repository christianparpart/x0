/* <HttpResponse.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef x0_response_h
#define x0_response_h (1)

#include <x0/http/HttpHeader.h>
#include <x0/http/HttpConnection.h>
#include <x0/http/HttpError.h>
#include <x0/http/HttpRequest.h>
#include <x0/io/Source.h>
#include <x0/io/BufferSource.h>
#include <x0/io/FilterSource.h>
#include <x0/io/ChainFilter.h>
#include <x0/Property.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <functional>
#include <string>
#include <vector>
#include <algorithm>

namespace x0 {

//! \addtogroup core
//@{

class HttpRequest;

/**
 * \brief HTTP response object.
 *
 * This response contains all information of data to be sent back to the requesting client.
 *
 * A response contains of three main data sections:
 * <ul>
 *   <li>response status</li>
 *   <li>response headers</li>
 *   <li>response body</li>
 * </ul>
 *
 * The response status determins wether the request could be fully handled or 
 * for whatever reason not.
 *
 * The response headers is a list of key/value pairs of standard HTTP/1.1 including application
 * dependant fields.
 *
 * The response body contains the actual content, and must be exactly as large as specified in
 * the "Content-Length" response header.
 *
 * However, if no "Content-Length" response header is specified, it is assured that 
 * this response will be the last response being transmitted through this very connection,
 * though, having keep-alive disabled.
 * The response status line and headers transmission will be started automatically as soon as the
 * first write occurs.
 * If this response meant to contain no body, then the transmit operation may be started explicitely.
 *
 * \note All response headers and status information <b>must</b> be fully
 * defined before the first content write operation.
 *
 * \see HttpResponse::finish(), HttpRequest, HttpConnection, HttpServer
 */
class X0_API HttpResponse
{
public:
	class header_list // {{{
	{
	public:
		typedef std::vector<HttpResponseHeader> impl_type;
		typedef impl_type::iterator iterator;
		typedef impl_type::const_iterator const_iterator;

	private:
		impl_type list_;

	public:
		std::size_t size() const
		{
			return list_.size();
		}

		bool contains(const std::string& name) const
		{
			for (std::size_t i = 0, e = list_.size(); i != e; ++i)
				if (strcasecmp(list_[i].name.c_str(), name.c_str()) == 0)
					return true;

			return false;
		}

		std::size_t find(const std::string& name) const
		{
			for (std::size_t i = 0, e = list_.size(); i != e; ++i)
				if (strcasecmp(list_[i].name.c_str(), name.c_str()) == 0)
					return i;

			return (std::size_t) -1;
		}

		const std::string& operator[](std::size_t pos) const
		{
			return list_[pos].value;
		}

		std::string& operator[](std::size_t pos)
		{
			return list_[pos].value;
		}

		const std::string& operator[](const std::string& name) const
		{
			for (std::size_t i = 0, e = list_.size(); i != e; ++i)
				if (strcasecmp(list_[i].name.c_str(), name.c_str()) == 0)
					return list_[i].value;

			static std::string not_found;
			return not_found;
		}

		std::string& operator[](const std::string& name)
		{
			std::size_t e = list_.size();

			for (std::size_t i = 0; i != e; ++i)
				if (strcasecmp(list_[i].name.c_str(), name.c_str()) == 0)
					return list_[i].value;

			list_.push_back(HttpResponseHeader(name, std::string()));
			return list_[e].value;
		}

		void push_back(const std::string& name, const std::string& value)
		{
			list_.push_back(HttpResponseHeader(name, value));
		}

		void overwrite(const std::string& name, const std::string& value)
		{
			operator[](name) = value;
		}

		void append(const std::string& name, const std::string& value)
		{
			// TODO append value to the header with name or create one if not yet available.
		}

		void remove(const std::string& name)
		{
#if 0
			//! \bug doesn't really work: it actually removes \p name but doubles another entry ("Vary" in our test-case)
			std::remove_if(begin(), end(), [&](const HttpResponseHeader& i) -> bool {
				return strcasecmp(i.name.c_str(), name.c_str()) == 0;
			});
#else
			for (iterator i = begin(), e = end(); i != e; ++i)
			{
				if (strcasecmp(i->name.c_str(), name.c_str()) == 0)
				{
					list_.erase(i);
					return;
				}
			}
#endif
		}

		// iterators
		iterator begin() { return list_.begin(); }
		iterator end() { return list_.end(); }

#if GCC_VERSION(4, 5)
		const_iterator cbegin() const { return list_.cbegin(); }
		const_iterator cend() const { return list_.cend(); }
#endif

	public:
		const std::string& operator()(const std::string& name) const
		{
			return operator[](name);
		}

		void operator()(const std::string& name, const std::string& value)
		{
			operator[](name) = value;
		}
	}; // }}}

private:
	/// pre-computed string representations of status codes, ready to be used by serializer
	static char status_codes[512][4];

	/// reference to the connection this response belongs to.
	HttpConnection *connection_;

	/// reference to the related request.
	HttpRequest *request_;

	// state whether response headers have been already sent or not.
	bool headers_sent_;

public:
	explicit HttpResponse(HttpConnection *connection, http_error status = static_cast<http_error>(0));
	~HttpResponse();

	/** retrieves a reference to the corresponding request object. */
	HttpRequest *request() const;

	/// HTTP response status code.
	http_error status;

	/// the headers to be included in the response.
	header_list headers;

	const std::string& header(const std::string& name) const;

	/** returns true in case serializing the response has already been started, that is, headers has been sent out already. */
	bool headers_sent() const;

	bool content_forbidden() const;

	void write(const SourcePtr& source, const CompletionHandlerType& handler);

	void finish();

private:
	void onWriteHeadersComplete(int ec, const SourcePtr& content, const CompletionHandlerType& handler);
	void writeContent(const SourcePtr& content, const CompletionHandlerType& handler);

	static void initialize();

	friend class HttpServer;
	friend class HttpConnection;

public:
	static std::string status_str(http_error status);

	ChainFilter filters;

private:
	SourcePtr serialize();

	SourcePtr make_default_content();

	void onFinished(int ec);
};

// {{{ inline implementation
inline HttpRequest *HttpResponse::request() const
{
	return request_;
}

inline bool HttpResponse::headers_sent() const
{
	return headers_sent_;
}

/** write given source to response content and invoke the completion handler when done.
 *
 * \param content the content (chunk) to push to the client
 * \param handler completion handler to invoke when source has been fully flushed or if an error occured
 *
 * \note this implicitely flushes the response-headers if not yet done, thus, making it impossible to modify them after this write.
 */
inline void HttpResponse::write(const SourcePtr& content, const CompletionHandlerType& handler)
{
	if (headers_sent_)
		writeContent(content, handler);
	else
		connection_->writeAsync(serialize(), 
			std::bind(&HttpResponse::onWriteHeadersComplete, this, std::placeholders::_1, content, handler));
}

/** is invoked as completion handler when sending response headers. */
inline void HttpResponse::onWriteHeadersComplete(int ec, const SourcePtr& content, const CompletionHandlerType& handler)
{
	headers_sent_ = true;

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

inline void HttpResponse::writeContent(const SourcePtr& content, const CompletionHandlerType& handler)
{
	if (filters.empty())
		connection_->writeAsync(content, handler);
	else
		connection_->writeAsync(std::make_shared<FilterSource>(content, filters, false), handler);
}

/** checks wether given code MUST NOT have a response body. */
inline bool HttpResponse::content_forbidden() const
{
	return x0::content_forbidden(status);
}

inline const std::string& HttpResponse::header(const std::string& name) const
{
	std::size_t pos = headers.find(name);
	if (pos != (std::size_t) -1)
		return headers[pos];

	static std::string not_found;
	return not_found;
}
// }}}

//@}

} // namespace x0

#endif
