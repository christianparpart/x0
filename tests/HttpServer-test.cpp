#include <gtest/gtest.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpClient.h>
#include <x0/Buffer.h>
#include <iostream>
#include <fstream>
#include <ev++.h>

using namespace x0;

class HttpServerTest : public ::testing::Test
{
public:
    HttpServerTest();

    void SetUp();
    void TearDown();

    void request(const std::string& method, const std::string& path,
        const std::initializer_list<std::pair<std::string, std::string>>& headers, const Buffer& content,
        HttpClient::ResponseHandler callback);

private:
    struct ev::loop_ref loop_;
    IPAddress host_;
    int port_;
    HttpServer* http_;
};

HttpServerTest::HttpServerTest() :
    loop_(ev::default_loop()),
    host_("127.0.0.1"),
    port_(8080),
    http_(nullptr)
{
}

void HttpServerTest::SetUp()
{
    http_ = new HttpServer(loop_);
}

void HttpServerTest::TearDown()
{
    delete http_;
    http_ = nullptr;
}

void HttpServerTest::request(const std::string& method, const std::string& path,
    const std::initializer_list<std::pair<std::string, std::string>>& headers, const Buffer& content,
    HttpClient::ResponseHandler callback)
{
    std::unordered_map<std::string, std::string> headerMap;
    for (auto item: headers)
        headerMap[item.first] = item.second;

    HttpClient::request(host_, port_, method, path, headerMap, content, callback);
}

TEST_F(HttpServerTest, DISABLED_Get)
{
    HttpClient::HeaderMap headers;
    Buffer body;

    HttpClient::request(IPAddress("127.0.0.1"), 8080, "GET", "/", {{"Foo", "bar"}, {"User-Agent", "HttpClient/1.0"}}, body,
    [&](HttpClientError ec, int status, const HttpClient::HeaderMap& headers, const BufferRef& content) {
        ASSERT_EQ(200, status);
        ASSERT_EQ(0, content.size());
    });
}

TEST_F(HttpServerTest, DISABLED_DirectoryTraversal)
{
    // TODO
}
