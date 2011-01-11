#! /usr/bin/env lua

request = {}

request.create = function(method, path, protocol)
{
	o = {}

	o.method = method;
	o.path = path;
	o.protocol = protocol;

	return o;
}

connection = {}
connection.new = function(hostname, port)
{
	o = {}
	o.hostname = hostname;
	o.port = port;
	return o;
}
connection.

function test_one()
{
	r = request:create "GET", "/", "HTTP/1.1";
	r.add_header ("Host", "localhost");

	c = connection:new ("localhost", 8080);
	c.write(r:to_s);
}
