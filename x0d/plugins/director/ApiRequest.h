/* <plugins/director/ApiRequest.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include "Backend.h"
#include "Director.h"
#include "HealthMonitor.h"

#include <x0/Buffer.h>
#include <x0/TimeSpan.h>
#include <x0/CustomDataMgr.h>
#include <x0/http/HttpRequest.h>

class DirectorPlugin;
class Director;

typedef std::unordered_map<std::string, Director*> DirectorMap;

enum class HttpMethod
{
	Unknown,

	// HTTP
	GET,
	PUT,
	POST,
	DELETE,
	CONNECT,

	// WebDAV
	MKCOL,
	MOVE,
	COPY,
	LOCK,
	UNLOCK,
};

class ApiRequest :
	public x0::CustomData
{
private:
	DirectorMap* directors_;
	x0::HttpRequest* request_;
	HttpMethod method_;
	x0::BufferRef path_;
	std::vector<BufferRef> tokens_;
	x0::Buffer body_;
	std::unordered_map<std::string, std::string> args_;

public:
	ApiRequest(DirectorMap* directors, x0::HttpRequest* r, const x0::BufferRef& path);
	~ApiRequest();

	static bool process(DirectorMap* directors, x0::HttpRequest* r, const x0::BufferRef& path);

protected:
	Director* findDirector(const x0::BufferRef& name);
	bool hasParam(const std::string& key) const;
	bool loadParam(const std::string& key, bool& result);
	bool loadParam(const std::string& key, int& result);
	bool loadParam(const std::string& key, size_t& result);
	bool loadParam(const std::string& key, float& result);
	bool loadParam(const std::string& key, x0::TimeSpan& result);
	bool loadParam(const std::string& key, BackendRole& result);
	bool loadParam(const std::string& key, HealthMonitor::Mode& result);
	bool loadParam(const std::string& key, std::string& result);
	bool loadParam(const std::string& key, TransferMode& result);

private:
	HttpMethod requestMethod() const { return method_; }

	void start();
	void onBodyChunk(const x0::BufferRef& chunk);
	void parseBody();
	bool process();

	// index
	bool processIndex();
	bool index();

	// director
	bool processDirector();
	bool show(Director* director);
	bool show(Backend* backend);
	bool update(Director* director);
	bool lock(bool enable, Director* director);

	// backend
	bool processBackend();
	bool createBackend(Director* director);
	bool update(Backend* backend, Director* director);
	bool lock(bool enable, Backend* backend, Director* director);
	bool destroy(Backend* backend, Director* director);

	// bucket
	bool processBucket();
	void processBucket(Director* director);
	bool createBucket(Director* director);
	void show(RequestShaper::Node* bucket);
	void update(RequestShaper::Node* bucket, Director* director);


	// helper
	static std::vector<x0::BufferRef> tokenize(const x0::BufferRef& input, const std::string& delimiter);
	bool resourceNotFound(const std::string& name, const std::string& value);
	bool badRequest(const char* msg = nullptr);
};
