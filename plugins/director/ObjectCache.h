#pragma once
/* <plugins/director/ObjectCache.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/Buffer.h>
#include <x0/TimeSpan.h>
#include <x0/DateTime.h>
#include <x0/http/HttpStatus.h>
#include <x0/io/Filter.h>
#include <tbb/concurrent_hash_map.h>
#include <unordered_map>
#include <string>
#include <atomic>
#include <utility>
#include <list>
#include <cstdint>

namespace x0 {
	class HttpRequest;
	class JsonWriter;
}

/**
 * Response Message Object Cache.
 *
 * Used to cache response messages.
 *
 * Concurrent access is generally (to be) supported
 * by using concurrent hash map as central cache store.
 *
 * Each method passing an HTTP request usually has to be
 * invoked from within the requests thread context.
 */
class ObjectCache {
public:
	class Builder;
	class Object;
	class VaryingObject;

protected:
	typedef tbb::concurrent_hash_map<std::string, Object*> ObjectMap;

	bool enabled_;
	bool deliverActive_;
	bool deliverShadow_;
	bool lockOnUpdate_;
	x0::TimeSpan updateLockTimeout_;
	std::string defaultKey_;
	x0::TimeSpan defaultTTL_;
	x0::TimeSpan defaultShadowTTL_;
	std::atomic<unsigned long long> cacheHits_;       //!< Total number of cache hits.
	std::atomic<unsigned long long> cacheShadowHits_; //!< Total number hits against shadow objects.
	std::atomic<unsigned long long> cacheMisses_;     //!< Total number of cache misses.
	std::atomic<unsigned long long> cachePurges_;     //!< Explicit purges.
	std::atomic<unsigned long long> cacheExpiries_;   //!< Automatic expiries.

	ObjectMap objects_;

public:
	ObjectCache();
	~ObjectCache();

	/**
	 * Global flag to either enable or disable object caching.
	 */
	bool enabled() const { return enabled_; }
	void setEnabled(bool value) { enabled_ = value; }

	/**
	 * Time to wait on an object that's currently being updated.
	 *
	 * A value of Zero means, that we will not wait at all and deliver the stale version instead.
	 */
	x0::TimeSpan updateLockTimeout() const { return updateLockTimeout_; }
	void setUpdateLockTimeout(const x0::TimeSpan& value) { updateLockTimeout_ = value; }

	bool lockOnUpdate() const { return lockOnUpdate_; }
	void setLockOnUpdate(bool value) { lockOnUpdate_ = value; }

	/**
	 * Boolean flag to either use the cache to accelerate backend traffic or not.
	 */
	bool deliverActive() const { return deliverActive_; }
	void setDeliverActive(bool value) { deliverActive_ = value; }

	/**
	 * Tests whether or not the object cache should be used to serve stale content over failure response.
	 *
	 * \retval true Stale content should be delivered instead of failure responses.
	 * \retval false No stale content may be delivered but the actual failure response is delivered instead.
	 */
	bool deliverShadow() const { return deliverShadow_; }
	void setDeliverShadow(bool value) { deliverShadow_ = value; }

	/**
	 * Default TTL (time to life) value a cache object is considered valid.
	 */
	x0::TimeSpan defaultTTL() const { return defaultTTL_; }
	void setDefaultTTL(const x0::TimeSpan& value) { defaultTTL_ = value; }

	/**
	 * Default TTL (time to life) value a stale cache object may held in the store.
	 */
	x0::TimeSpan defaultShadowTTL() const { return defaultShadowTTL_; }
	void setDefaultShadowTTL(const x0::TimeSpan& value) { defaultShadowTTL_ = value; }

	unsigned long long cacheHits() const { return cacheHits_; }
	unsigned long long cacheShadowHits() const { return cacheShadowHits_; }
	unsigned long long cacheMisses() const { return cacheShadowHits_; }
	unsigned long long cachePurges() const { return cachePurges_; }
	unsigned long long cacheExpiries() const { return cacheExpiries_; }

	/**
	 * Attempts to serve the request from cache.
	 *
	 * \retval true request is being served from cache.
	 * \retval false request is \b NOT being served from cache, but an object construction listener has been installed to populate the cache object.
	 *
	 * \note Invoked from within the thread context of its served request.
	 *
	 * \see deliverShadow(x0::HttpRequest* r, const std::string& cacheKey);
	 */
	bool deliverActive(x0::HttpRequest* r, const std::string& cacheKey);

	/**
	 * Attempts to serve the request from cache if available, doesn't do anything else otherwise.
	 *
	 * \retval true request is being served from cache.
	 * \retval false request is \b NOT being served from cache, and no other modifications has been done either.
	 *
	 * \see deliverActive(x0::HttpRequest* r, const std::string& cacheKey);
	 */
	bool deliverShadow(x0::HttpRequest* r, const std::string& cacheKey);

public:
	/**
	 * Searches for a cache object for read access.
	 *
	 * @param cacheKey the unique cache key identifying the cache object to be acquired.
	 * @param callback a callback invoked with the cache-object.
	 *
	 * @retval true the cache-object was found.
	 * @retval false the cache-object was not found.
	 *
	 * If the cache-object was not found, the callback must still be invoked with
	 * its argument set to NULL.
	 */
	bool find(const std::string& cacheKey, const std::function<void(Object*)>& callback);

	/**
	 * Searches for a cache object for read/write access.
	 *
	 * @param cacheKey the unique cache key identifying the cache object to be acquired.
	 * @param callback a callback invoked with the cache-object.
	 *
	 * @retval true the cache-object was found.
	 * @retval false the cache-object was not found.
	 *
	 * The callback gets itself two arguments, first, the cache-object, and second,
	 * a boolean indicating if this object got just created with this call,
	 * or false if this cache-object has been already in the cache store.
	 */
	bool acquire(const std::string& cacheKey, const std::function<void(Object*, bool)>& callback);

	/**
	 * Actively purges (expires) a cache object from the store.
	 *
	 * This does not mean that the cache object is gone from the store. It is usually
	 * just flagged as invalid, so it can still be served if stale content is requested.
	 *
	 * \retval true Object found.
	 * \retval false Object not found.
	 */
	bool purge(const std::string& cacheKey);

	/**
	 *
	 * Globally purges (expires) the complete cache store.
	 *
	 * This does not mean that the cache objects are gone from the store. They are usually
	 * just flagged as invalid, so they can still be served if stale content is requested.
	 */
	void clear(bool physically);

	void writeJSON(x0::JsonWriter& json) const;
};

/**
 * A cache-object that contains a response message.
 */
class ObjectCache::Object {
public:
	Object(ObjectCache* store, const std::string& cacheKey);
	~Object();

	/*!
	 * Updates given object by hooking into the requests output stream.
	 *
	 * \param r The request whose response will be used to update this object.
	 *
	 * When invoking update with a request while another request is already in progress
	 * of updating this object, this request will be put onto the interest list instead
	 * and will get the response once the initial request's response has arrived.
	 *
	 * This method must be invoked from within thre requests thread context.
	 *
	 * @retval true This request is not used for updating the object and got just enqueued for the response instead.
	 * @retval false This request is being used for updating the object. So further processing must occur.
	 */
	bool update(x0::HttpRequest* r);

	//! The object's state.
	enum State {
		Spawning,  //!< the cache object is just being constructed, and not yet completed.
		Active,    //!< the cache object is valid and ready to be delivered
		Stale,     //!< the cache object is stale
		Updating,  //!< the cache object is stale but is already in progress of being updated.
	};

	State state() const;

	bool isSpawning() const { return state() == Spawning; }
	bool isStale() const { return state() == Stale; }

	/**
	 * creation time of given cache object or the time it was last updated.
	 */
	x0::DateTime ctime() const;

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
	 */
	void deliver(x0::HttpRequest* r);

	//! marks object as expired but does not destruct it from the store.
	void expire();

	std::string requestHeader(const std::string& name) const;

private:
	inline void internalDeliver(x0::HttpRequest* r);

	struct Buffer { // {{{
		x0::DateTime ctime;
		x0::HttpStatus status;
		std::list<std::pair<std::string, std::string>> headers;
		x0::Buffer body;
		size_t hits;

		Buffer() :
			ctime(),
			status(x0::HttpStatus::Undefined),
			headers(),
			body(),
			hits(0)
		{
		}

		void reset() {
			status = x0::HttpStatus::Undefined;
			headers.clear();
			body.clear();
			hits = 0;
		}
	}; // }}}

	void postProcess();
	void addHeaders(x0::HttpRequest* r, bool hit);
	void destroy();

	// used at construction/update state
	void append(const x0::BufferRef& ref);
	void commit(); // FIXME: why not used ATM ?

	const Buffer& frontBuffer() const { return buffer_[bufferIndex_]; }
	Buffer& frontBuffer() { return buffer_[bufferIndex_]; }
	Buffer& backBuffer() { return buffer_[!bufferIndex_]; }

	void swapBuffers() {
		bufferIndex_ = !bufferIndex_;
		backBuffer().reset();
	}

private:
	ObjectCache* store_;
	std::string cacheKey_;

	x0::HttpRequest* request_;              //!< either NULL or pointer to request currently updating this object.
	std::list<x0::HttpRequest*> interests_; //!< list of requests that have to deliver this object ASAP.

	State state_;

	std::unordered_map<std::string, std::string> requestHeaders_;

	size_t bufferIndex_;
	Buffer buffer_[2];

	friend class ObjectCache;
};

class ObjectCache::VaryingObject {
public:
	/**
	 * selects a cache object
	 */
	Object* select(const x0::HttpRequest* request) const;

	bool testMatch(const x0::HttpRequest* request, const Object* object) const;

private:
	std::list<std::string> requestHeaders_;
	std::list<Object*> objects_;
};

class ObjectCache::Builder : public x0::Filter {
private:
	Object* object_;

public:
	explicit Builder(Object* object);

	virtual x0::Buffer process(const x0::BufferRef& chunk);
};

inline x0::JsonWriter& operator<<(x0::JsonWriter& json, const ObjectCache& cache) {
	cache.writeJSON(json);
	return json;
}

std::string to_s(ObjectCache::Object::State value);
