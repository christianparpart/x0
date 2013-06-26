/* <plugins/director/ObjectCache.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include "ObjectCache.h"
#include "RequestNotes.h"
#include <x0/io/BufferRefSource.h>

using namespace x0;

#define TRACE(n, msg...) ((void*)0) /*!*/

// TODO thread safety
// TODO test cache validity when output compression is enabled (or other output filters are applied).

// {{{ ObjectCache::Object
ObjectCache::Object::Object()
{
}

ObjectCache::Object::~Object()
{
}
// }}}
// {{{ ObjectCache
ObjectCache::ObjectCache() :
	enabled_(true),
	deliverActive_(true),
	deliverShadow_(true),
	lockOnUpdate_(true),
	updateLockTimeout_(TimeSpan::fromSeconds(10)),
	defaultTTL_(TimeSpan::fromSeconds(20)), //TimeSpan::Zero),
	defaultShadowTTL_(TimeSpan::Zero),
	cacheHits_(0),
	cacheShadowHits_(0),
	cacheMisses_(0),
	cachePurges_(0)
{
}

ObjectCache::~ObjectCache()
{
}
// }}}
// {{{ MallocStore
MallocStore::MallocStore() :
	ObjectCache()
{
}

MallocStore::~MallocStore()
{
}

bool MallocStore::find(const std::string& cacheKey, const std::function<void(ObjectCache::Object*)>& callback)
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

bool MallocStore::acquire(const std::string& cacheKey, const std::function<void(ObjectCache::Object*, bool)>& callback)
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

void MallocStore::purge(const std::string& cacheKey)
{
	ObjectMap::accessor accessor;
	if (objects_.find(accessor, cacheKey)) {
		accessor->second->expire();
	}
}

void MallocStore::clear(bool physically)
{
	if (physically) {
		for (auto object: objects_) {
			delete object.second;
		}
		objects_.clear();
	} else {
		for (auto object: objects_) {
			object.second->expire();
		}
	}
}
// }}}
// {{{ MallocStore::Builder
class MallocStore::Builder : public x0::Filter {
private:
	Object* object_;

public:
	explicit Builder(Object* object);

	virtual x0::Buffer process(const x0::BufferRef& chunk) final;
};

MallocStore::Builder::Builder(Object* object) :
	object_(object)
{
}

Buffer MallocStore::Builder::process(const BufferRef& chunk)
{
	if (object_) {
		if (!chunk.empty()) {
			object_->backBuffer().body.push_back(chunk);
			TRACE(3, "MallocStore.Builder.process(): %zu bytes\n", chunk.size());
		}
	}

	return Buffer(chunk);
}
// }}}
// {{{ MallocStore::Object
MallocStore::Object::Object(MallocStore* store, const std::string& cacheKey) :
	store_(store),
	cacheKey_(cacheKey),
	request_(nullptr),
	interests_(),
	state_(Spawning),
	bufferIndex_()
{
}

MallocStore::Object::~Object()
{
}

void MallocStore::Object::postProcess()
{
	TRACE(3, "Object.postProcess() status: %d\n", request_->status);
	request_->outputFilters.push_back(std::make_shared<Builder>(this));
	request_->onRequestDone.connect<MallocStore::Object, &MallocStore::Object::commit>(this);
	backBuffer().status = request_->status;

	for (auto header: request_->responseHeaders) {
//		if (unlikely(iequals(header.name, "Set-Cookie"))) {
//			request_->log(Severity::info, "Caching requested but origin server provides uncacheable response header, Set-Cookie. Do not cache.");
//			destroy();
//			return;
//		}

//		if (unlikely(iequals(header.name, "Cache-Control") && iequals(header.value, "no-cache"))) {
//			destroy();
//			return;
//		}

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
}

std::string to_s(ObjectCache::Object::State value)
{
	static const char* ss[] = { "Spawning", "Active", "Stale", "Updating" };
	return ss[(size_t)value];
}

void MallocStore::Object::addHeaders(HttpRequest* r, bool hit)
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
	r->responseHeaders.push_back("X-Cache-Age", buf);
}

MallocStore::Object::State MallocStore::Object::state() const
{
	return state_;
}

void MallocStore::Object::append(const x0::BufferRef& ref)
{
	backBuffer().body.push_back(ref);
}

void MallocStore::Object::commit()
{
	TRACE(3, "Object: commit\n");
	backBuffer().ctime = request_->connection.worker().now();
	swapBuffers();
	request_ = nullptr;
	state_ = Active;

	auto pendingRequests = std::move(interests_);
	size_t i = 0;
	(void) i;
	for (auto request: pendingRequests) {
		TRACE(3, "commit: deliver to pending request %zu\n", ++i);
		request->post([=]() {
			deliver(request);
		});
	}
}

DateTime MallocStore::Object::ctime() const
{
	return frontBuffer().ctime;
}

/*!
 * Updates given object by hooking into the requests output stream.
 *
 * Any other requests that will be added to the interests list past this method call
 * will be invoked as soon as this object has become valid again.
 *
 * \param r the request whose response will be used to update this object.
 */
void MallocStore::Object::update(HttpRequest* r)
{
	++store_->cacheMisses_;

	if (state_ != Spawning)
		state_ = Updating;

	TRACE(3, "Object.update() -> %s\n", to_s(state_).c_str());
	request_ = r;
	request_->onPostProcess.connect<Object, &Object::postProcess>(this);
}

void MallocStore::Object::enqueue(HttpRequest* r)
{
	// TODO: honor updateLockTimeout

	interests_.push_back(r);
}

/*!
 * Delivers this object to the given HTTP client.
 *
 * It directly serves the object if it is in state \c Active or \c Stale.
 * If the object is in state \p Updating or \p Spawning otherwise, it will
 * append the HTTP request to the list of pending clients and wait there
 * for cache object completion and will be served as sonn as this object
 * became ready.
 *
 * \param r the request this object should be served as response to.
 *
 */
void MallocStore::Object::deliver(x0::HttpRequest* r)
{
	switch (state()) {
		case Spawning:
			++store_->cacheHits_;
			enqueue(r);
			TRACE(3, "Object.deliver(): adding R to interest list (%zu) as we're still Spawning\n", interests_.size());
			break;
		case Updating:
			if (store_->lockOnUpdate()) {
				++store_->cacheHits_;
				enqueue(r);
				TRACE(3, "Object.deliver(): adding R to interest list (%zu) as we're still Updating\n", interests_.size());
				break;
			}
			// fall through
		case Stale:
			++store_->cacheShadowHits_;
			internalDeliver(r);
			break;
		case Active: {
			++store_->cacheHits_;
			internalDeliver(r);
			break;
		}
	}
}

void MallocStore::Object::internalDeliver(x0::HttpRequest* r)
{
	++frontBuffer().hits;

	TRACE(3, "Object.deliver(): hit %zu, state %s\n", frontBuffer().hits, to_s(state_).c_str());

	r->status = frontBuffer().status;

	for (auto header: frontBuffer().headers)
		r->responseHeaders.push_back(header.first, header.second);

	addHeaders(r, true);

	if (!equals(r->method, "HEAD"))
		r->write<BufferRefSource>(frontBuffer().body);

	r->finish();
}

void MallocStore::Object::expire()
{
	++store_->cachePurges_;
	state_ = Stale;
}

void MallocStore::Object::destroy()
{
	if (store_->objects_.erase(cacheKey_)) {
		delete this;
	}
}
// }}}
