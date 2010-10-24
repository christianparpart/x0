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
	class HeaderList // {{{
	{
	public:
		struct Item { // {{{
			std::string name;
			std::string value;
			Item *prev;
			Item *next;

			Item(const std::string& _name, const std::string& _value, Item *_prev, Item *_next) :
				name(_name), value(_value), prev(_prev), next(_next)
			{
				if (prev)
					prev->next = this;

				if (next)
					next->prev = this;
			}

			~Item()
			{
				fprintf(stderr, "Item('%s', '%s')\n", name.c_str(), value.c_str());
			}

			void dump()
			{
				fprintf(stderr, "Item('%s', '%s', %p, %p)\n", name.c_str(), value.c_str(), (void*)prev, (void*)next);
			}
		};
		// }}}

		class iterator { // {{{
		private:
			Item *current_;

		public:
			iterator() :
				current_(NULL)
			{}

			explicit iterator(Item *item) :
				current_(item)
			{}

			Item& operator*()
			{
				return *current_;
			}

			Item& operator->()
			{
				return *current_;
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
		Item *first_;
		Item *last_;

	public:
		HeaderList() :
			size_(0), first_(NULL), last_(NULL)
		{}

		~HeaderList()
		{
			while (first_) {
				delete unlinkItem(first_);
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
			for (const Item *i = first_; i != NULL; i = i->next)
				if (strcasecmp(i->name.c_str(), name.c_str()) == 0)
					return true;

			return false;
		}

		void push_back(const std::string& name, const std::string& value)
		{
			last_ = new Item(name, value, last_, NULL);

			if (first_ == NULL)
				first_ = last_;

			++size_;
		}

		Item **findItem(const std::string& name)
		{
			Item **item = &first_;

			while (*item != NULL)
			{
				if (strcasecmp((*item)->name.c_str(), name.c_str()) == 0)
					return item;

				item = &(*item)->next;
			}

			return NULL;
		}

		void overwrite(const std::string& name, const std::string& value)
		{
			Item **item = findItem(name);

			if (item && *item)
				(*item)->value = value;
			else
				push_back(name, value);
		}

		const std::string& operator[](const std::string& name) const
		{
			Item **item = const_cast<HeaderList *>(this)->findItem(name);
			if (item)
				return (*item)->value;

			static std::string not_found;
			return not_found;
		}

		std::string& operator[](const std::string& name)
		{
			Item **item = findItem(name);
			if (item)
				return (*item)->value;

			static std::string not_found;
			return not_found;
		}

		void append(const std::string& name, const std::string& value)
		{
			// TODO append value to the header with name or create one if not yet available.
		}

		Item *unlinkItem(Item *item)
		{
			Item *prev = item->prev;
			Item *next = item->next;

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
			Item **item = findItem(name);
			if (item)
			{
				delete unlinkItem(*item);
			}
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
	explicit HttpResponse(HttpConnection *connection, HttpError status = static_cast<HttpError>(0));
	~HttpResponse();

	/** retrieves a reference to the corresponding request object. */
	HttpRequest *request() const;

	/// HTTP response status code.
	HttpError status;

	/// the headers to be included in the response.
	HeaderList headers;

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
	static std::string status_str(HttpError status);

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
	return headers[name];
}
// }}}

//@}

} // namespace x0

#endif
