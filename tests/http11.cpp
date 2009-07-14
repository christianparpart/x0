#include <initializer_list>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <strings.h>

std::string hostname_("localhost");
std::string port_("8080");

#ifndef NDEBUG
# define NDEBUG 1
#endif

// {{{ response
class response
{
public:
	void add_header(const std::string& line);

	bool has_header(const std::string& key);
	bool header_equals(const std::string& key, const std::string& value);
	bool header_contains(const std::string& key, const std::string& value);

	// response status
	std::string protocol;
	int status;
	std::string status_text;

	std::map<std::string, std::string> headers;

	// response body
	std::string content;
};

void response::add_header(const std::string& line)
{
	std::size_t n = line.find(":");

	if (n != std::string::npos)
	{
		std::string key(line.substr(0, n++));

		while (std::isspace(line[n]))
			++n;

		std::string value(line.substr(n));

#ifndef NDEBUG
		std::cout << "> " << key << ": " << value << std::endl;
#endif

		headers[key] = value;
	}
}

bool response::has_header(const std::string& key)
{
	for (auto i = headers.begin(), e = headers.end(); i != e; ++i)
	{
		if (strcasecmp(i->first.c_str(), key.c_str()) == 0)
		{
			return true;
		}
	}
	return false;
}

bool response::header_equals(const std::string& key, const std::string& value)
{
	for (auto i = headers.begin(), e = headers.end(); i != e; ++i)
	{
		if (strcasecmp(i->first.c_str(), key.c_str()) == 0)
		{
			std::string v(i->second);
			if (strcasecmp(v.c_str(), value.c_str()) == 0)
//			if (strcasecmp(i->second.c_str(), value.c_str()) == 0)
			{
				return true;
			}
		}
	}
	return false;
}

bool response::header_contains(const std::string& key, const std::string& value)
{
	for (auto i = headers.begin(), e = headers.end(); i != e; ++i)
	{
		if (strcasecmp(i->first.c_str(), key.c_str()) == 0)
		{
			if (strcasestr(i->second.c_str(), value.c_str()) != 0)
			{
				return true;
			}
		}
	}
	return false;
}
// }}}

// {{{ request(...)
response request(const std::string& line, const std::vector<std::pair<std::string, std::string>>& headers)
{
	using boost::asio::ip::tcp;

	response result;
	boost::asio::io_service ios;

	tcp::resolver resolver(ios);
	tcp::resolver::query query(hostname_, boost::lexical_cast<std::string>(port_));
	tcp::resolver::iterator endpoint_iterator(resolver.resolve(query));
	tcp::resolver::iterator end;

	tcp::socket socket(ios);
	boost::system::error_code error = boost::asio::error::host_not_found;

	while (error && endpoint_iterator != end)
	{
		socket.close();
		socket.connect(*endpoint_iterator++, error);
	}
	if (error)
		throw boost::system::system_error(error);

	boost::asio::streambuf request;
	std::ostream request_stream(&request);
	request_stream << line << "\r\n";

#ifndef NDEBUG
	std::cout << "< " << line << std::endl;
#endif

	for (auto i = headers.begin(), e = headers.end(); i != e; ++i)
	{
		request_stream << i->first << ": " << i->second << "\r\n";
#ifndef NDEBUG
		std::cout << "< " << i->first << ": " << i->second << std::endl;
#endif
	}

	std::string host(hostname_);
	if (port_ != "80")
	{
		host += ":";
		host += port_;
	}
	request_stream << "Host: " << host << "\r\n";
#ifndef NDEBUG
	std::cout << "< Host: " << host << std::endl;
#endif

	request_stream << "\r\n";

	boost::asio::write(socket, request);

	boost::asio::streambuf response;

	// response status
	boost::asio::read_until(socket, response, "\r\n");
	std::istream response_stream(&response);
	response_stream >> result.protocol;
	response_stream >> result.status;
	std::getline(response_stream, result.status_text);

	// response header(s)
	boost::asio::read_until(socket, response, "\r\n\r\n");
	std::string value;
	while (std::getline(response_stream, value) && value != "\r")
		result.add_header(value.substr(0, value.length() - 1));

	// response body
	std::stringstream content;
	if (response.size() > 0)
		content << &response;

	while (boost::asio::read(socket, response, boost::asio::transfer_at_least(1), error))
		content << &response;

	if (error != boost::asio::error::eof)
		throw boost::system::system_error(error);

	result.content = content.str();

	return result;
}
response request(const std::string& line)
{
	return request(line, {} );
}
// }}}

std::pair<std::string, std::string> header(const std::string& key, const std::string& value)
{
	return std::make_pair(key, value);
}

class http11 :
	public CPPUNIT_NS::TestFixture
{
public:
	CPPUNIT_TEST_SUITE(http11);
		CPPUNIT_TEST(_404);
		CPPUNIT_TEST(range1);
		CPPUNIT_TEST(range2);
		CPPUNIT_TEST(range3);
		CPPUNIT_TEST(range4);
	CPPUNIT_TEST_SUITE_END();

private:
	void _404()
	{
		auto response = request("GET /404 HTTP/1.0", {header("foo", "bar"), header("com", "tar")});

		CPPUNIT_ASSERT(response.protocol == "HTTP/1.0");
		CPPUNIT_ASSERT(response.status == 404);
		CPPUNIT_ASSERT(response.has_header("Content-Type"));
		CPPUNIT_ASSERT(response.content.size() > 0);
	}

	// {{{ ranges
	void range1()
	{
		auto response = request("GET /12345.txt HTTP/1.1", { header("Range", "bytes=0-3") });

		CPPUNIT_ASSERT(response.status == 206);
		CPPUNIT_ASSERT(response.header_equals("Content-Length", "4"));
		CPPUNIT_ASSERT(response.content == "1234");
	}

	void range2()
	{
		auto response = request("GET /12345.txt HTTP/1.1", { header("Range", "bytes=1-1") });

		CPPUNIT_ASSERT(response.status == 206);
		CPPUNIT_ASSERT(response.header_equals("Content-Length", "1"));
		CPPUNIT_ASSERT(response.content == "2");
	}

	void range3()
	{
		auto response = request("GET /12345.txt HTTP/1.1", { header("Range", "bytes=0-4") });

		CPPUNIT_ASSERT(response.status == 206);
		CPPUNIT_ASSERT(response.header_equals("Content-Length", "5"));
		CPPUNIT_ASSERT(response.content == "12345");
	}

	void range4()
	{
		auto response = request("GET /12345.txt HTTP/1.1", { header("Range", "bytes=2-2,1-1,0-0") });

		CPPUNIT_ASSERT(response.status == 206);
		CPPUNIT_ASSERT(response.header_contains("Content-Type", "multipart/byteranges"));
		CPPUNIT_ASSERT(response.content.find("Content-Type: text/plain") != std::string::npos);
	}
	// }}}
};

CPPUNIT_TEST_SUITE_REGISTRATION(http11);













