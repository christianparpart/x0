#pragma once

#include "Backend.h"
#include "HealthMonitor.h"

#include <x0/Buffer.h>
#include <x0/BufferRef.h>
#include <x0/CustomDataMgr.h>
#include <x0/http/HttpRequest.h>

// index:   GET    /
// get:     GET    /:director_id
//
// enable:  UNLOCK /:director_id/:backend_id
// disable: LOCK   /:director_id/:backend_id
//
// create:  PUT    /:director_id/:backend_id
// update:  POST   /:director_id/:backend_id
// delete:  DELETE /:director_id/:backend_id

class DirectorPlugin;
class Director;

typedef std::unordered_map<std::string, Director*> DirectorMap;

class ApiReqeust :
	public x0::CustomData
{
private:
	DirectorMap* directors_;
	x0::HttpRequest* request_;
	x0::BufferRef path_;
	x0::Buffer body_;
	std::unordered_map<std::string, std::string> args_;

public:
	ApiReqeust(DirectorMap* directors, x0::HttpRequest* r, const x0::BufferRef& path);
	~ApiReqeust();

	static bool process(DirectorMap* directors, x0::HttpRequest* r, const x0::BufferRef& path);

protected:
	Director* findDirector(const std::string& name);
	bool hasParam(const std::string& key) const;
	bool loadParam(const std::string& key, bool& result);
	bool loadParam(const std::string& key, int& result);
	bool loadParam(const std::string& key, size_t& result);
	bool loadParam(const std::string& key, Backend::Role& result);
	bool loadParam(const std::string& key, HealthMonitor::Mode& result);
	bool loadParam(const std::string& key, std::string& result);

private:
	void start();
	void onBodyChunk(const x0::BufferRef& chunk);
	bool process();
	void parseBody();

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
	std::vector<std::string> tokenize(
		const std::string& input,
		const std::string& delimiter,
		char escapeChar = '\\',
		bool exclusive = false);
};
