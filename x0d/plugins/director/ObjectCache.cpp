/* <plugins/director/ObjectCache.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include "ObjectCache.h"
#include "RequestNotes.h"
#include "Director.h"
#include <x0/io/BufferRefSource.h>
#include <x0/http/HttpRequest.h>
#include <x0/DebugLogger.h>

/*
 * List of messages headers that must be taken into account when checking for freshness.
 *
 * REQUEST HEADERS
 * - If-Range
 * - If-Match
 * - If-None-Match
 * - If-Modified-Since
 * - If-Unmodified-Since
 *
 * RESPONSE HEADERS
 * - Last-Modified
 * - ETag
 * - Expires
 * - Cache-Control
 * - Pragma (token: "no-cache")
 * - Vary (list of request headers that make a response unique in addition to its cache key, or "*" for literally all request headers)
 */

// TODO thread safety
// TODO test cache validity when output compression is enabled (or other output filters are applied).

using namespace x0;

#define TRACE(rn, n, msg...) do { \
	if ((rn) && (rn)->request != nullptr) { \
		LogMessage m(Severity::debug ## n, msg); \
		m.addTag("director-cache"); \
		rn->request->log(std::move(m)); \
	} else { \
		X0_DEBUG("director-cache", (n), msg); \
	} \
} while (0)

// {{{ ObjectCache::ConcreteObject
ObjectCache::ConcreteObject::ConcreteObject(ObjectCache* store, const std::string& cacheKey) :
    Object(),
	store_(store),
	cacheKey_(cacheKey),
    state_(Spawning),
    requestNotes_(nullptr),
	interests_(),
	requestHeaders_(),
	bufferIndex_()
{
    TRACE(requestNotes_, 2, "ConcreteObject(key: '%s')", cacheKey_.c_str());
}

ObjectCache::ConcreteObject::~ConcreteObject()
{
    TRACE(requestNotes_, 2, "~ConcreteObject()");
}

std::string ObjectCache::ConcreteObject::requestHeader(const std::string& name) const
{
	return "";
}

void ObjectCache::ConcreteObject::postProcess()
{
	TRACE(requestNotes_, 3, "ConcreteObject.postProcess() status: %d", requestNotes_->request->status);

	for (auto header: requestNotes_->request->responseHeaders) {
		TRACE(requestNotes_, 3, "ConcreteObject.postProcess() %s: %s", header.name.c_str(), header.value.c_str());
		if (unlikely(iequals(header.name, "Set-Cookie"))) {
			requestNotes_->request->log(Severity::info, "Caching requested but origin server provides uncacheable response header, Set-Cookie. Do not cache.");
			destroy();
			return;
		}

		if (unlikely(iequals(header.name, "Cache-Control"))) {
			if (iequals(header.value, "no-cache")) {
				TRACE(requestNotes_, 2, "\"Cache-Control: no-cache\" detected. do not record object then.");
				destroy();
				return;
			}
			// TODO we can actually cache it if it's max-age=N is N > 0 for exact N seconds.
		}

        if (unlikely(iequals(header.name, "Pragma"))) {
            if (iequals(header.value, "no-cache")) {
                TRACE(requestNotes_, 2, "\"Pragma: no-cache\" detected. do not record object then.");
                destroy();
                return;
            }
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

	addHeaders(requestNotes_->request, false);

	requestNotes_->request->outputFilters.push_back(std::make_shared<Builder>(this));
	requestNotes_->request->onRequestDone.connect<ObjectCache::ConcreteObject, &ObjectCache::ConcreteObject::commit>(this);
	backBuffer().status = requestNotes_->request->status;
}

std::string to_s(ObjectCache::ConcreteObject::State value)
{
	static const char* ss[] = { "Spawning", "Active", "Stale", "Updating" };
	return ss[(size_t)value];
}

void ObjectCache::ConcreteObject::addHeaders(HttpRequest* r, bool hit)
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

void ObjectCache::ConcreteObject::append(const x0::BufferRef& ref)
{
	backBuffer().body.push_back(ref);
}

void ObjectCache::ConcreteObject::commit()
{
	TRACE(requestNotes_, 2, "ConcreteObject: commit");
	backBuffer().ctime = requestNotes_->request->connection.worker().now();
	swapBuffers();
	requestNotes_ = nullptr;
	state_ = Active;

	auto pendingRequests = std::move(interests_);
	size_t i = 0;
	(void) i;
	for (auto rn: pendingRequests) {
		TRACE(requestNotes_, 3, "commit: deliver to pending request %zu", ++i);
		rn->request->post([=]() {
			deliver(rn);
		});
	}
}

DateTime ObjectCache::ConcreteObject::ctime() const
{
	return frontBuffer().ctime;
}

ObjectCache::ConcreteObject* ObjectCache::ConcreteObject::select(const RequestNotes* rn) const
{
    return const_cast<ConcreteObject*>(this);
}

bool ObjectCache::ConcreteObject::update(RequestNotes* rn)
{
	TRACE(rn, 3, "ConcreteObject.update() -> %s", to_s(state_).c_str());

	if (state_ != Spawning)
		state_ = Updating;

	if (!requestNotes_) {
		// this is the first interest request, so it must be responsible for updating this object, too.
        requestNotes_ = rn;
		rn->request->onPostProcess.connect<ConcreteObject, &ConcreteObject::postProcess>(this);
		return false;
	} else {
		// we have already some request that's updating this object, so add us to the interest list
		// and wait for the response.
		interests_.push_back(rn); // TODO honor updateLockTimeout
        TRACE(rn, 3, "Concurrent update detected. Enqueuing interest (%zu).", interests_.size());
		return true;
	}
}

void ObjectCache::ConcreteObject::deliver(RequestNotes* rn)
{
	internalDeliver(rn);
}

void ObjectCache::ConcreteObject::internalDeliver(RequestNotes* rn)
{
    HttpRequest* r = rn->request;

	++frontBuffer().hits;

	TRACE(rn, 3, "ConcreteObject.deliver(): hit %zu, state %s", frontBuffer().hits, to_s(state_).c_str());

	r->status = frontBuffer().status;

	for (auto header: frontBuffer().headers)
		r->responseHeaders.push_back(header.first, header.second);

	addHeaders(r, true);

    char slen[16];
    snprintf(slen, sizeof(slen), "%zu", frontBuffer().body.size());
    r->responseHeaders.overwrite("Content-Length", slen);

	if (!equals(r->method, "HEAD"))
		r->write<BufferRefSource>(frontBuffer().body);

	r->finish();
}

void ObjectCache::ConcreteObject::expire()
{
	state_ = Stale;
}

void ObjectCache::ConcreteObject::destroy()
{
    // reschedule all pending interested clients on this object as we're going to get destroyed.
    // the reschedule should not include the cache again.
    auto pendingRequests = std::move(interests_);
    for (auto rn: pendingRequests) {
        rn->cacheIgnore = true;
        store_->director_->reschedule(rn);
    }

	if (store_->objects_.erase(cacheKey_)) {
		delete this;
	} else {
        assert(!"Mission impossible engaged on us. Trying to destroy a cached object that is not (anymore?) in the store.");
    }
}
// }}}
// {{{ ObjectCache::VaryingObject
ObjectCache::ConcreteObject* ObjectCache::VaryingObject::select(const RequestNotes* rn) const
{
	for (const auto& object: objects_)
		if (testMatch(rn, object))
			return object;

	return nullptr;
}

bool ObjectCache::VaryingObject::testMatch(const RequestNotes* rn, const ConcreteObject* object) const
{
    HttpRequest* request = rn->request;

	for (const auto& name: requestHeaders_)
		if (request->requestHeader(name) != object->requestHeader(name))
			return false;

	return true;
}
// }}}
// {{{ ObjectCache
ObjectCache::ObjectCache(Director* director) :
    director_(director),
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
			accessor->second = new ConcreteObject(this, cacheKey);
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

void ObjectCache::expireAll()
{
    for (auto object: objects_) {
        ++cachePurges_;
        object.second->expire();
    }
}

void ObjectCache::purgeAll()
{
    for (auto object: objects_) {
        delete object.second;
    }

    cachePurges_ += objects_.size();
    objects_.clear();
}

bool ObjectCache::deliverActive(RequestNotes* rn)
{
	if (!deliverActive())
		return false;

	bool processed = false;

	acquire(rn->cacheKey, [&](Object* someObject, bool created) {
		if (created) {
			// cache object did not exist and got just created for by this request
			//printf("Director.processCacheObject: object created\n");
			++cacheMisses_;
			processed = someObject->update(rn);
		} else if (someObject) {
            ConcreteObject* object = someObject->select(rn);

			const DateTime now = rn->request->connection.worker().now();
			const DateTime expiry = object->ctime() + rn->cacheTTL;

			if (expiry < now)
				object->expire();

			switch (object->state()) {
				case ConcreteObject::Spawning:
					++cacheHits_;
					processed = object->update(rn);
					break;
				case ConcreteObject::Updating:
					if (lockOnUpdate()) {
						++cacheHits_;
						processed = !object->update(rn);
					} else {
						++cacheShadowHits_;
						processed = true;
						object->deliver(rn);
						//TRACE(rn, 3, "ConcreteObject.deliver(): deliver shadow object");
					}
					break;
				case ConcreteObject::Stale:
					++cacheMisses_;
					processed = object->update(rn);
					break;
				case ConcreteObject::Active:
					processed = true;
					++cacheHits_;
					object->deliver(rn);
					break;
			}
		}
	});

	return processed;
}

bool ObjectCache::deliverShadow(RequestNotes* rn)
{
	if (deliverShadow()) {
		if (find(rn->cacheKey,
				[rn, this](Object* object)
				{
					if (object) {
						++cacheShadowHits_;
						rn->request->responseHeaders.push_back("X-Director-Cache", "shadow");
						object->deliver(rn);
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
        .name("default-ttl")(defaultTTL().totalSeconds())
        .name("default-shadow-ttl")(defaultShadowTTL().totalSeconds())
        .beginObject("stats")
            .name("misses")(cacheMisses())
            .name("hits")(cacheHits())
            .name("shadow-hits")(cacheShadowHits())
            .name("purges")(cachePurges())
        .endObject()
    .endObject();
}
// }}}
// {{{ ObjectCache::Builder
ObjectCache::Builder::Builder(ConcreteObject* object) :
	object_(object)
{
}

Buffer ObjectCache::Builder::process(const BufferRef& chunk)
{
	if (object_) {
		TRACE(object_->requestNotes_, 3, "ObjectCache.Builder.process(): %zu bytes", chunk.size());
		if (!chunk.empty()) {
			object_->backBuffer().body.push_back(chunk);
		}
	}

	return Buffer(chunk);
}
// }}}
