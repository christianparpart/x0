/* <plugins/director/ObjectCache.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Buffer.h>
#include <x0/TimeSpan.h>
#include <x0/DateTime.h>
#include <x0/http/HttpStatus.h>
#include <tbb/concurrent_hash_map.h>
#include <string>
#include <atomic>
#include <utility>
#include <list>
#include <cstdint>

namespace x0 {
	class HttpRequest;
	class JsonWriter;
}

struct RequestNotes;

class ObjectCache {
public:
	struct Stats {
		uint64_t activeObjects;
		uint64_t shadowObjects;
		uint64_t purges;
	};

	class Object {
	public:
		Object();
		virtual ~Object();

		virtual void update(x0::HttpRequest* r) = 0;

		enum State {
			Spawning,  //!< the cache object is just being constructed, and not yet completed.
			Active,    //!< the cache object is valid and ready to be delivered
			Stale,     //!< the cache object is stale
			Updating,  //!< the cache object is stale but is already in progress of being updated.
		};

		virtual State state() const = 0;

		bool isSpawning() const { return state() == Spawning; }
		bool isStale() const { return state() == Stale; }

		//! creation time of given object.
		virtual x0::DateTime ctime() const = 0;

		//! delivers given object.
		virtual void deliver(x0::HttpRequest* r) = 0;

		//! marks object as expired but does not destruct it from the store.
		virtual void expire() = 0;
	};

	class Blob;

protected:
	bool enabled_;
	bool deliverActive_;
	bool deliverShadow_;
	bool lockOnUpdate_;
	x0::TimeSpan updateLockTimeout_;
	std::string defaultKey_;
	x0::TimeSpan defaultTTL_;
	x0::TimeSpan defaultShadowTTL_;
	std::atomic<unsigned long long> cacheHits_;
	std::atomic<unsigned long long> cacheShadowHits_;
	std::atomic<unsigned long long> cacheMisses_;
	std::atomic<unsigned long long> cachePurges_; //!< explicit purges
	std::atomic<unsigned long long> cacheExpiries_; //!< automatic expiries

public:
	ObjectCache();
	virtual ~ObjectCache();

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
	 */
	virtual bool find(const std::string& cacheKey, const std::function<void(Object*)>& callback) = 0;

	/**
	 * Searches for a cache object for read/write access.
	 */
	virtual bool acquire(const std::string& cacheKey, const std::function<void(Object*, bool)>& callback) = 0;

	/**
	 * Actively purges (expires) a cache object from the store.
	 *
	 * This does not mean that the cache object is gone from the store. It is usually
	 * just flagged as invalid, so it can still be served if stale content is requested.
	 *
	 * \retval true Object found.
	 * \retval false Object not found.
	 */
	virtual bool purge(const std::string& cacheKey) = 0;

	/**
	 *
	 * Globally purges (expires) the complete cache store.
	 *
	 * This does not mean that the cache objects are gone from the store. They are usually
	 * just flagged as invalid, so they can still be served if stale content is requested.
	 */
	virtual void clear(bool physically) = 0;

	virtual void writeJSON(x0::JsonWriter& json) const;
};

inline x0::JsonWriter& operator<<(x0::JsonWriter& json, const ObjectCache& cache) {
	cache.writeJSON(json);
	return json;
}

class MallocStore :
	public ObjectCache
{
private:
	class Builder;

	/*!
	 * Double-buffered HTTP response object cache.
	 */
	class Object : public ObjectCache::Object {
	public:
		Object(MallocStore* store, const std::string& cacheKey);
		~Object();

		void update(x0::HttpRequest* r);

		State state() const;

		x0::DateTime ctime() const;
		void deliver(x0::HttpRequest* r);
		void expire();

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

		void enqueue(x0::HttpRequest* r);
		void postProcess();
		void addHeaders(x0::HttpRequest* r, bool hit);
		void destroy();

		// used at construction/update state
		void append(const x0::BufferRef& ref);
		void commit();

		const Buffer& frontBuffer() const { return buffer_[bufferIndex_]; }
		Buffer& frontBuffer() { return buffer_[bufferIndex_]; }
		Buffer& backBuffer() { return buffer_[!bufferIndex_]; }

		void swapBuffers() {
			bufferIndex_ = !bufferIndex_;
			backBuffer().reset();
		}

	private:
		MallocStore* store_;
		std::string cacheKey_;
		x0::HttpRequest* request_;              //!< either NULL or pointer to request currently updating this object.
		std::list<x0::HttpRequest*> interests_; //!< list of requests that have to deliver this object ASAP.

		State state_;

		size_t bufferIndex_;
		Buffer buffer_[2];

		friend class MallocStore;
	};

	typedef tbb::concurrent_hash_map<std::string, Object*> ObjectMap;

	ObjectMap objects_;

public:
	MallocStore();
	~MallocStore();

	bool find(const std::string& cacheKey, const std::function<void(ObjectCache::Object*)>& callback);
	bool acquire(const std::string& cacheKey, const std::function<void(ObjectCache::Object*, bool /*created*/)>& callback);
	bool purge(const std::string& cacheKey);
	void clear(bool physically);

private:
	void commit(Object* object);
};

std::string to_s(ObjectCache::Object::State value);

#if 0
class ObjectCache::Blob: public ObjectCache::Object { // {{{
private:
	struct X0_PACKED Header {
		uint32_t status;
		uint32_t headerSize;
		ev::tstamp ctime;
	};
	x0::Buffer message_;

	const Header& header() const { return *reinterpret_cast<const Header*>(message_.data()); }
	Header& header() { return *reinterpret_cast<Header*>(message_.data()); }

public:
	Blob(x0::HttpStatus status, x0::HttpRequest::HeaderList* responseHeaders);
	~Blob();

	//! access to the raw data representing the cache object.
	const x0::Buffer& data() const { return message_; }

	x0::DateTime ctime() const { return x0::DateTime(header().ctime); }
	x0::HttpStatus messageStatus() const { return static_cast<x0::HttpStatus>(header().status); }
	x0::BufferRef messageHeaders() const { return message_.ref(sizeof(Header), header().headerSize); }
	x0::BufferRef messageBody() const { return message_.ref(sizeof(Header) + header().headerSize); }
}; // }}}
#endif
