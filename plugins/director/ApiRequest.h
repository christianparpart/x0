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

class ApiRequest :
	public x0::CustomData
{
private:
	DirectorMap* directors_;
	x0::HttpRequest* request_;
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
	void start();
	void onBodyChunk(const x0::BufferRef& chunk);
	void parseBody();
	bool process();
	bool processBucket();
	void processBucket(Director* director);
	bool processBackend();
	bool processDirector();
	bool processIndex();

	bool index();
	bool eventstream();
	bool get();
	bool lock(bool enable);
	bool create();
	bool update();
	bool updateDirector(Director* director);
	bool updateBackend(Director* director, const std::string& name);
	bool destroy();

	// helper
	static std::vector<x0::BufferRef> tokenize(const x0::BufferRef& input, const std::string& delimiter);
	bool resourceNotFound(const std::string& name, const std::string& value);
};
