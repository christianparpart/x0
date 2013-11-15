/* <plugins/director/ObjectCache.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include "ObjectCache.h"
#include "RequestNotes.h"
#include <x0/io/BufferRefSource.h>
#include <x0/http/HttpRequest.h>
#include <x0/DebugLogger.h>

/*
 * List of messages headers that must be taken into account when checking for freshness.
 *
 * REQUEST HEADERS
 * - If-Modified-Since
 * - If-None-Match
 *
 * RESPONSE HEADERS
 * - Last-Modified
 * - ETag
 * - Expires
 * - Cache-Control
 * - Vary (list of request headers that make a response unique in addition to its cache key, or "*" for literally all request headers)
 */

// TODO thread safety
// TODO test cache validity when output compression is enabled (or other output filters are applied).

using namespace x0;

#define TRACE(r, n, msg...) do { \
	if (r) { \
		LogMessage m(Severity::debug ## n, msg); \
		m.addTag("director-cache"); \
		r->log(std::move(m)); \
	} else { \
		X0_DEBUG("director-cache", (n), msg); \
	} \
} while (0)

// {{{ ObjectCache::Object
ObjectCache::Object::Object(ObjectCache* store, const std::string& cacheKey) :
	store_(store),
	cacheKey_(cacheKey),
	request_(nullptr),
	interests_(),
	state_(Spawning),
	requestHeaders_(),
	bufferIndex_()
{
}

ObjectCache::Object::~Object()
{
}

std::string ObjectCache::Object::requestHeader(const std::string& name) const
{
	return "";
}

void ObjectCache::Object::postProcess()
{
	TRACE(request_, 3, "Object.postProcess() status: %d", request_->status);

	for (auto header: request_->responseHeaders) {
		TRACE(request_, 3, "Object.postProcess() %s: %s", header.name.c_str(), header.value.c_str());
		if (unlikely(iequals(header.name, "Set-Cookie"))) {
			request_->log(Severity::info, "Caching requested but origin server provides uncacheable response header, Set-Cookie. Do not cache.");
			destroy();
			return;
		}

		if (unlikely(iequals(header.name, "Cache-Control"))) {
			if (iequals(header.value, "no-cache")) {
				TRACE(request_, 2, "Cache-Control detected. do not record object then.");
				destroy();
				return;
			}
			// TODO we can actually cache it if it's max-age=N is N > 0 for exact N seconds.
		}

		if (unlikely(iequals(header.name, "X-Director-Cache")))
			continue;

		bool skip = false;
//		for (auto ignore: notes->cacheHeaderIgnores) {
//			if (iequals(header.name, ignore)) {
//				skip = true;
//				break;
//			}
//		}

		if (!skip) {
			backBuffer().headers.push_back(std::make_pair(header.name, header.value));
		}
	}

	addHeaders(request_, false);

	request_->outputFilters.push_back(std::make_shared<Builder>(this));
	request_->onRequestDone.connect<ObjectCache::Object, &ObjectCache::Object::commit>(this);
	backBuffer().status = request_->status;
}

std::string to_s(ObjectCache::Object::State value)
{
	static const char* ss[] = { "Spawning", "Active", "Stale", "Updating" };
	return ss[(size_t)value];
}

void ObjectCache::Object::addHeaders(HttpRequest* r, bool hit)
{
	static const char* ss[] = {
		"miss",  // Spawning
		"hit",   // Active
		"stale", // Stale
		"stale-updating", // Updating
	};

	r->responseHeaders.push_back("X-Cache-Lookup", ss[(size_t)state_]);

	char buf[32];
	snprintf(buf, sizeof(buf), "%zu", hit ? frontBuffer().hits : 0);
	r->responseHeaders.push_back("X-Cache-Hits", buf);

	TimeSpan age(r->connection.worker().now() - frontBuffer().ctime);
	snprintf(buf, sizeof(buf), "%zu", (size_t)(hit ? age.totalSeconds() : 0));
	r->responseHeaders.push_back("Age", buf);
}

ObjectCache::Object::State ObjectCache::Object::state() const
{
	return state_;
}

void ObjectCache::Object::append(const x0::BufferRef& ref)
{
	backBuffer().body.push_back(ref);
}

void ObjectCache::Object::commit()
{
	TRACE(request_, 2, "Object: commit");
	backBuffer().ctime = request_->connection.worker().now();
	swapBuffers();
	request_ = nullptr;
	state_ = Active;

	auto pendingRequests = std::move(interests_);
	size_t i = 0;
	(void) i;
	for (auto request: pendingRequests) {
		TRACE(request_, 3, "commit: deliver to pending request %zu", ++i);
		request->post([=]() {
			deliver(request);
		});
	}
}

DateTime ObjectCache::Object::ctime() const
{
	return frontBuffer().ctime;
}

bool ObjectCache::Object::update(HttpRequest* r)
{
	TRACE(request_, 3, "Object.update() -> %s", to_s(state_).c_str());

	if (state_ != Spawning)
		state_ = Updating;

	if (!request_) {
		// this is the first interest request, so it must be responsible for updating this object, too.
		request_ = r;
		r->onPostProcess.connect<Object, &Object::postProcess>(this);
		return false;
	} else {
		// we have already some request that's updating this object, so add us to the interest list
		// and wait for the response.
		interests_.push_back(r); // TODO honor updateLockTimeout
		return true;
	}
}

void ObjectCache::Object::deliver(x0::HttpRequest* r)
{
	internalDeliver(r);
}

void ObjectCache::Object::internalDeliver(x0::HttpRequest* r)
{
	++frontBuffer().hits;

	TRACE(r, 3, "Object.deliver(): hit %zu, state %s", frontBuffer().hits, to_s(state_).c_str());

	r->status = frontBuffer().status;

	for (auto header: frontBuffer().headers)
		r->responseHeaders.push_back(header.first, header.second);

	addHeaders(r, true);

	if (!equals(r->method, "HEAD"))
		r->write<BufferRefSource>(frontBuffer().body);

	r->finish();
}

void ObjectCache::Object::expire()
{
	state_ = Stale;
}

void ObjectCache::Object::destroy()
{
	if (store_->objects_.erase(cacheKey_)) {
		delete this;
	}
}
// }}}
// {{{ ObjectCache::VaryingObject
ObjectCache::Object* ObjectCache::VaryingObject::select(const x0::HttpRequest* request) const
{
	for (const auto& object: objects_)
		if (testMatch(request, object))
			return object;

	return nullptr;
}

bool ObjectCache::VaryingObject::testMatch(const x0::HttpRequest* request, const Object* object) const
{
	for (const auto& name: requestHeaders_)
		if (request->requestHeader(name) != object->requestHeader(name))
			return false;

	return true;
}
// }}}
// {{{ ObjectCache
ObjectCache::ObjectCache() :
	enabled_(true),
	deliverActive_(true),
	deliverShadow_(true),
	lockOnUpdate_(true),
	updateLockTimeout_(TimeSpan::fromSeconds(10)),
	defaultKey_("%h%r%q"),
	defaultTTL_(TimeSpan::fromSeconds(20)), //TimeSpan::Zero),
	defaultShadowTTL_(TimeSpan::Zero),
	cacheHits_(0),
	cacheShadowHits_(0),
	cacheMisses_(0),
	cachePurges_(0),
	cacheExpiries_(0),
	objects_()
{
}

ObjectCache::~ObjectCache()
{
}

bool ObjectCache::find(const std::string& cacheKey, const std::function<void(ObjectCache::Object*)>& callback)
{
	if (enabled()) {
		ObjectMap::const_accessor accessor;
		if (objects_.find(accessor, cacheKey)) {
			callback(accessor->second);
			return true;
		}
	}
	callback(nullptr);
	return false;
}

bool ObjectCache::acquire(const std::string& cacheKey, const std::function<void(ObjectCache::Object*, bool)>& callback)
{
	if (enabled()) {
		ObjectMap::accessor accessor;
		if (objects_.insert(accessor, cacheKey)) {
			accessor->second = new Object(this, cacheKey);
			callback(accessor->second, true);
			return true;
		} else {
			callback(accessor->second, false);
			return false;
		}
	} else {
		callback(nullptr, false);
		return false;
	}
}

bool ObjectCache::purge(const std::string& cacheKey)
{
	bool result = false;
	ObjectMap::accessor accessor;

	if (objects_.find(accessor, cacheKey)) {
		++cachePurges_;
		accessor->second->expire();
		result = true;
	}

	return result;
}

void ObjectCache::clear(bool physically)
{
	if (physically) {
		for (auto object: objects_) {
			delete object.second;
		}
		cachePurges_ += objects_.size();
		objects_.clear();
	} else {
		for (auto object: objects_) {
			++cachePurges_;
			object.second->expire();
		}
	}
}

bool ObjectCache::deliverActive(x0::HttpRequest* r, const std::string& cacheKey)
{
	if (!deliverActive())
		return false;

	bool processed = false;

	acquire(cacheKey, [&](Object* object, bool created) {
		if (created) {
			// cache object did not exist and got just created for by this request
			//printf("Director.processCacheObject: object created\n");
			++cacheMisses_;
			processed = object->update(r);
		} else if (object) {
			const DateTime now = r->connection.worker().now();
			const DateTime expiry = object->ctime() + defaultTTL();

			if (expiry < now)
				object->expire();

			switch (object->state()) {
				case Object::Spawning:
					++cacheHits_;
					processed = object->update(r);
					break;
				case Object::Updating:
					if (lockOnUpdate()) {
						++cacheHits_;
						processed = !object->update(r);
					} else {
						++cacheShadowHits_;
						processed = true;
						object->deliver(r);
						//TRACE(3, "Object.deliver(): deliver shadow object");
					}
					break;
				case Object::Stale:
					++cacheMisses_;
					processed = object->update(r);
					break;
				case Object::Active:
					processed = true;
					++cacheHits_;
					object->deliver(r);
					break;
			}
		}
	});

	return processed;
}

bool ObjectCache::deliverShadow(x0::HttpRequest* r, const std::string& cacheKey)
{
	if (deliverShadow()) {
		if (find(cacheKey,
				[&](Object* object)
				{
					if (object) {
						++cacheShadowHits_;
						r->responseHeaders.push_back("X-Director-Cache", "shadow");
						object->deliver(r);
					}
				}
			))
		{
			return true;
		}
	}

	return false;
}

void ObjectCache::writeJSON(x0::JsonWriter& json) const
{
	json.beginObject()
		.name("enabled")(enabled())
		.name("deliver-active")(deliverActive())
		.name("deliver-shadow")(deliverShadow())
		.name("misses")(cacheMisses())
		.name("hits")(cacheHits())
		.name("shadow-hits")(cacheShadowHits())
		.name("purges")(cachePurges())
		.endObject();
}
// }}}
// {{{ ObjectCache::Builder
ObjectCache::Builder::Builder(Object* object) :
	object_(object)
{
}

Buffer ObjectCache::Builder::process(const BufferRef& chunk)
{
	if (object_) {
		TRACE(object_->request_, 3, "ObjectCache.Builder.process(): %zu bytes", chunk.size());
		if (!chunk.empty()) {
			object_->backBuffer().body.push_back(chunk);
		}
	}

	return Buffer(chunk);
}
// }}}
