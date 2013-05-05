#include <gtest/gtest.h>
#include <x0/http/HttpServer.h>
#include <x0/Buffer.h>
#include <iostream>
#include <fstream>
#include <ev++.h>

class HttpServerTest : public ::testing::Test
{
public:
	HttpServerTest();

	void SetUp();
	void TearDown();

	void DirectoryTraversal();

private:
	struct ev::loop_ref loop_;
	x0::HttpServer* http_;
};

HttpServerTest::HttpServerTest() :
	loop_(ev::default_loop(0)),
	http_(nullptr)
{
	x0::FlowRunner::initialize();
}

void HttpServerTest::SetUp()
{
	http_ = new x0::HttpServer(loop_);
}

void HttpServerTest::TearDown()
{
	delete http_;
	http_ = nullptr;
}

TEST_F(HttpServerTest, DirectoryTraversal)
{
	// TODO
}
