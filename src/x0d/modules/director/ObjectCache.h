// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <base/Buffer.h>
#include <base/Duration.h>
#include <base/UnixTime.h>
#include <base/io/Filter.h>
#include <xzero/HttpStatus.h>
#include <tbb/concurrent_hash_map.h>
#include <unordered_map>
#include <string>
#include <atomic>
#include <utility>
#include <vector>
#include <list>
#include <cstdint>

namespace base {
class JsonWriter;
}

namespace xzero {
class HttpRequest;
}

class Director;
class RequestNotes;

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
  class ConcreteObject;

 protected:
  typedef tbb::concurrent_hash_map<std::string, Object*> ObjectMap;

  Director* director_;
  bool enabled_;
  bool deliverActive_;
  bool deliverShadow_;
  bool lockOnUpdate_;
  base::Duration updateLockTimeout_;
  std::string defaultKey_;
  base::Duration defaultTTL_;
  base::Duration defaultShadowTTL_;
  std::atomic<unsigned long long> cacheHits_;  //!< Total number of cache hits.
  std::atomic<unsigned long long> cacheShadowHits_;  //!< Total number hits
                                                     //against shadow objects.
  std::atomic<unsigned long long> cacheMisses_;      //!< Total number of cache
                                                     //misses.
  std::atomic<unsigned long long> cachePurges_;      //!< Explicit purges.
  std::atomic<unsigned long long> cacheExpiries_;    //!< Automatic expiries.

  ObjectMap objects_;

 public:
  explicit ObjectCache(Director* director);
  ~ObjectCache();

  Director* director() const { return director_; }

  /**
   * Global flag to either enable or disable object caching.
   */
  bool enabled() const { return enabled_; }
  void setEnabled(bool value) { enabled_ = value; }

  /**
   * Time to wait on an object that's currently being updated.
   *
   * A value of Zero means, that we will not wait at all and deliver the stale
   *version instead.
   */
  base::Duration updateLockTimeout() const { return updateLockTimeout_; }
  void setUpdateLockTimeout(const base::Duration& value) {
    updateLockTimeout_ = value;
  }

  bool lockOnUpdate() const { return lockOnUpdate_; }
  void setLockOnUpdate(bool value) { lockOnUpdate_ = value; }

  /**
   * Boolean flag to either use the cache to accelerate backend traffic or not.
   */
  bool deliverActive() const { return deliverActive_; }
  void setDeliverActive(bool value) { deliverActive_ = value; }

  /**
   * Tests whether or not the object cache should be used to serve stale content
   *over failure response.
   *
   * \retval true Stale content should be delivered instead of failure
   *responses.
   * \retval false No stale content may be delivered but the actual failure
   *response is delivered instead.
   */
  bool deliverShadow() const { return deliverShadow_; }
  void setDeliverShadow(bool value) { deliverShadow_ = value; }

  /**
   * Default TTL (time to life) value a cache object is considered valid.
   */
  base::Duration defaultTTL() const { return defaultTTL_; }
  void setDefaultTTL(const base::Duration& value) { defaultTTL_ = value; }

  /**
   * Default TTL (time to life) value a stale cache object may held in the
   * store.
   */
  base::Duration defaultShadowTTL() const { return defaultShadowTTL_; }
  void setDefaultShadowTTL(const base::Duration& value) {
    defaultShadowTTL_ = value;
  }

  unsigned long long cacheHits() const { return cacheHits_; }
  unsigned long long cacheShadowHits() const { return cacheShadowHits_; }
  unsigned long long cacheMisses() const { return cacheShadowHits_; }
  unsigned long long cachePurges() const { return cachePurges_; }
  unsigned long long cacheExpiries() const { return cacheExpiries_; }

  /**
   * Attempts to serve the request from cache.
   *
   * \retval true request is being served from cache.
   * \retval false request is \b NOT being served from cache, but an object
   *construction listener has been installed to populate the cache object.
   *
   * \note Invoked from within the thread context of its served request.
   *
   * \see deliverShadow(xzero::HttpRequest* r, const std::string& cacheKey);
   */
  bool deliverActive(RequestNotes* rn);

  /**
   * Attempts to serve the request from cache if available, doesn't do anything
   *else otherwise.
   *
   * \retval true request is being served from cache.
   * \retval false request is \b NOT being served from cache, and no other
   *modifications has been done either.
   *
   * \see deliverActive(xzero::HttpRequest* r, const std::string& cacheKey);
   */
  bool deliverShadow(RequestNotes* rn);

 public:
  /**
   * Searches for a cache object for read access.
   *
   * @param cacheKey the unique cache key identifying the cache object to be
   *acquired.
   * @param callback a callback invoked with the cache-object.
   *
   * @retval true the cache-object was found.
   * @retval false the cache-object was not found.
   *
   * If the cache-object was not found, the callback must still be invoked with
   * its argument set to NULL.
   */
  bool find(const std::string& cacheKey,
            const std::function<void(Object*)>& callback);

  /**
   * Searches for a cache object for read/write access.
   *
   * @param cacheKey the unique cache key identifying the cache object to be
   *acquired.
   * @param callback a callback invoked with the cache-object.
   *
   * @retval true the cache-object was found.
   * @retval false the cache-object was not found.
   *
   * The callback gets itself two arguments, first, the cache-object, and
   *second,
   * a boolean indicating if this object got just created with this call,
   * or false if this cache-object has been already in the cache store.
   */
  bool acquire(const std::string& cacheKey,
               const std::function<void(Object*, bool)>& callback);

  /**
   * Actively purges (expires) a cache object from the store.
   *
   * This does not mean that the cache object is gone from the store. It is
   *usually
   * just flagged as invalid, so it can still be served if stale content is
   *requested.
   *
   * \retval true Object found.
   * \retval false Object not found.
   */
  bool purge(const std::string& cacheKey);

  /**
   * Expires all cached objects without freeing their backing store.
   */
  void expireAll();

  /**
   * Purges all cached objects completely and frees up their backing store.
   */
  void purgeAll();

  void writeJSON(base::JsonWriter& json) const;
};

/**
 * A cache-object containing an HTTP response message, respecting the HTTP
 * <b>Vary</b> response header.
 */
class ObjectCache::Object {
 public:
  Object(ObjectCache* store, const std::string& cacheKey);
  ~Object();

  ObjectCache* store() const { return store_; }
  const std::string& cacheKey() const { return cacheKey_; }

  /**
   * Selects a cache-object based on the request's cache key and Vary header.
   */
  ConcreteObject* select(const RequestNotes* rn);

  bool update(RequestNotes* rn);
  void deliver(RequestNotes* rn);
  void expire();

  void destroy(ConcreteObject* concreteObject);

 private:
  ObjectCache* store_;
  std::string cacheKey_;
  std::list<std::string> requestHeaders_;  //!< list of all request header names
                                           //which value may <b>vary</b>.
  std::list<ConcreteObject*> objects_;     //!< list of objects for each
                                           //<b>variation</b>.

  friend class ConcreteObject;
};

/**
 * A cache-object that contains an HTTP response message.
 */
class ObjectCache::ConcreteObject {
 public:
  //! The object's state.
  enum State {
    Spawning,  //!< the cache object is just being constructed, and not yet
               //completed.
    Active,    //!< the cache object is valid and ready to be delivered
    Stale,     //!< the cache object is stale
    Updating,  //!< the cache object is stale but is already in progress of
               //being updated.
  };

  explicit ConcreteObject(Object* objectGroup);
  ~ConcreteObject();

  Object* object() const { return object_; }

  /*!
   * Updates given object by hooking into the requests output stream.
   *
   * \param r The request whose response will be used to update this object.
   *
   * When invoking update with a request while another request is already in
   *progress
   * of updating this object, this request will be put onto the interest list
   *instead
   * and will get the response once the initial request's response has arrived.
   *
   * This method must be invoked from within thre requests thread context.
   *
   * @retval true This request is not used for updating the object and got just
   *enqueued for the response instead.
   * @retval false This request is being used for updating the object. So
   *further processing must occur.
   */
  bool update(RequestNotes* rn);

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
  void deliver(RequestNotes* rn);

  /**
   * Marks object as expired but does not destruct it from the store.
   */
  void expire();

  State state() const { return state_; }

  bool isSpawning() const { return state() == Spawning; }
  bool isStale() const { return state() == Stale; }

  /**
   * creation time of given cache object or the time it was last updated.
   */
  base::UnixTime ctime() const;

  /**
   * Retrieves the value of a given request header.
   *
   */
  const std::string& varyingHeader(const base::BufferRef& name) const;

  const std::vector<std::pair<base::BufferRef, std::string>>& varyingHeaders()
      const {
    return frontBuffer().varyingHeaders;
  }

  /**
   * Tests whether given request matches this concrete cache object, according
   * to the Vary headers.
   */
  bool isMatch(const xzero::HttpRequest* r) const;

 private:
  inline void internalDeliver(RequestNotes* rn);

  struct Buffer {  // {{{
    base::UnixTime ctime;
    xzero::HttpStatus status;

    std::list<std::pair<std::string, std::string>> headers;
    std::vector<std::pair<base::BufferRef, std::string>> varyingHeaders;
    std::string etag;
    base::UnixTime mtime;
    base::Buffer body;
    size_t hits;

    Buffer()
        : ctime(),
          status(xzero::HttpStatus::Undefined),
          headers(),
          etag(),
          mtime(),
          body(),
          hits(0) {}

    void clear() {
      status = xzero::HttpStatus::Undefined;
      headers.clear();
      body.clear();
      hits = 0;
    }
  };  // }}}

  xzero::HttpStatus tryProcessClientCache(RequestNotes* rn);
  void postProcess();
  void addHeaders(xzero::HttpRequest* r, bool hit);
  void destroy();

  // used at construction/update state
  void append(const base::BufferRef& ref);

  /**
   * Invoked upon completion of an update process.
   *
   * @see HttpServer::onRequestDone
   */
  void commit();

  const Buffer& frontBuffer() const { return buffer_[bufferIndex_]; }
  Buffer& frontBuffer() { return buffer_[bufferIndex_]; }
  Buffer& backBuffer() { return buffer_[!bufferIndex_]; }

  void swapBuffers() {
    bufferIndex_ = !bufferIndex_;
    backBuffer().clear();
  }

 private:
  Object* object_;
  State state_;

  RequestNotes* requestNotes_;  //!< either NULL or pointer to request currently
                                //updating this object.
  std::list<RequestNotes*> interests_;  //!< list of requests that have to
                                        //deliver this object ASAP.

  size_t bufferIndex_;
  Buffer buffer_[2];

  friend class ObjectCache;
};

/**
 * HTTP response filter, used to populate a cache-object with a fresh response.
 */
class ObjectCache::Builder : public base::Filter {
 private:
  ConcreteObject* object_;

 public:
  explicit Builder(ConcreteObject* object);

  virtual base::Buffer process(const base::BufferRef& chunk);
};

inline base::JsonWriter& operator<<(base::JsonWriter& json,
                                  const ObjectCache& cache) {
  cache.writeJSON(json);
  return json;
}

std::string to_s(ObjectCache::ConcreteObject::State value);
