// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "ObjectCache.h"
#include "RequestNotes.h"
#include "Director.h"
#include <base/Tokenizer.h>
#include <base/io/BufferRefSource.h>
#include <xzero/HttpRequest.h>
#include <base/DebugLogger.h>

/*
 * List of messages headers that must be taken into account when checking for
 *freshness.
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
 * - Vary (list of request headers that make a response unique in addition to
 *its cache key, or "*" for literally all request headers)
 */

// TODO thread safety
// TODO test cache validity when output compression is enabled (or other output
// filters are applied).

using namespace base;
using namespace xzero;

#define TRACE(rn, n, msg...)                 \
  do {                                       \
    if ((rn) && (rn)->request != nullptr) {  \
      LogMessage m(Severity::trace##n, msg); \
      m.addTag("director-cache");            \
      rn->request->log(std::move(m));        \
    } else {                                 \
      XZERO_DEBUG("director-cache", (n), msg);  \
    }                                        \
  } while (0)

// {{{ ObjectCache::ConcreteObject
ObjectCache::ConcreteObject::ConcreteObject(Object* object)
    : object_(object),
      state_(Spawning),
      requestNotes_(nullptr),
      interests_(),
      bufferIndex_() {
  TRACE(requestNotes_, 2, "ConcreteObject(key: '$0')", object_->cacheKey());
}

ObjectCache::ConcreteObject::~ConcreteObject() {
  TRACE(requestNotes_, 2, "~ConcreteObject()");
}

bool ObjectCache::ConcreteObject::isMatch(const HttpRequest* r) const {
  // TODO: speed me up by pre-hashing before doing the full compare

  for (const auto& header : varyingHeaders())
    if (!iequals(r->requestHeader(header.first), header.second)) return false;

  return true;
}

void ObjectCache::ConcreteObject::postProcess() {
  TRACE(requestNotes_, 3, "ConcreteObject.postProcess() status: $0",
        requestNotes_->request->status);
  HttpRequest* r = requestNotes_->request;

  backBuffer().varyingHeaders.clear();

  for (const auto& header : requestNotes_->request->responseHeaders) {
    TRACE(requestNotes_, 3, "ConcreteObject.postProcess() $0: $1",
          header.name, header.value);
    if (unlikely(iequals(header.name, "Set-Cookie"))) {
      requestNotes_->request->log(Severity::info,
                                  "Caching requested but origin server "
                                  "provides uncacheable response header, "
                                  "Set-Cookie. Do not cache.");
      destroy();
      return;
    }

    if (unlikely(iequals(header.name, "Cache-Control"))) {
      if (iequals(header.value, "no-cache")) {
        TRACE(
            requestNotes_, 2,
            "\"Cache-Control: no-cache\" detected. do not record object then.");
        destroy();
        return;
      }
      // TODO we can actually cache it if it's max-age=N is N > 0 for exact N
      // seconds.
    }

    if (unlikely(iequals(header.name, "Pragma"))) {
      if (iequals(header.value, "no-cache")) {
        TRACE(requestNotes_, 2,
              "\"Pragma: no-cache\" detected. do not record object then.");
        destroy();
        return;
      }
    }

    if (unlikely(iequals(header.name, "X-Director-Cache"))) continue;

    if (iequals(header.name, "ETag")) backBuffer().etag = header.value;

    if (iequals(header.name, "Last-Modified"))
      backBuffer().mtime = UnixTime(header.value);

    if (iequals(header.name, "Vary")) {
      Tokenizer<BufferRef> st(
          BufferRef(header.value.c_str(), header.value.size()), ", \t\r\n");
      std::vector<BufferRef> theVaryingHeaders(std::move(st.tokenize()));

      for (const auto& theHeaderName : theVaryingHeaders) {
        backBuffer().varyingHeaders.push_back(std::make_pair(
            theHeaderName, r->requestHeader(theHeaderName).str()));
      }
    }

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

  requestNotes_->request->outputFilters.push_back(
      std::make_shared<Builder>(this));
  requestNotes_->request->onRequestDone.connect<
      ObjectCache::ConcreteObject, &ObjectCache::ConcreteObject::commit>(this);
  backBuffer().status = requestNotes_->request->status;
}

std::string to_s(ObjectCache::ConcreteObject::State value) {
  static const char* ss[] = {"Spawning", "Active", "Stale", "Updating"};
  return ss[(size_t)value];
}

void ObjectCache::ConcreteObject::addHeaders(HttpRequest* r, bool hit) {
  static const char* ss[] = {"miss",            // Spawning
                             "hit",             // Active
                             "stale",           // Stale
                             "stale-updating",  // Updating
  };

  r->responseHeaders.push_back("X-Cache-Lookup", ss[(size_t)state_]);

  char buf[32];
  snprintf(buf, sizeof(buf), "%zu", hit ? frontBuffer().hits : 0);
  r->responseHeaders.push_back("X-Cache-Hits", buf);

  Duration age(r->connection.worker().now() - frontBuffer().ctime);
  snprintf(buf, sizeof(buf), "%zu", (size_t)(hit ? age.totalSeconds() : 0));
  r->responseHeaders.push_back("Age", buf);
}

void ObjectCache::ConcreteObject::append(const BufferRef& ref) {
  backBuffer().body.push_back(ref);
}

void ObjectCache::ConcreteObject::commit() {
  TRACE(requestNotes_, 2, "ConcreteObject: commit");

  backBuffer().ctime = requestNotes_->request->connection.worker().now();

  if (backBuffer().mtime.unixtime() == 0)
    backBuffer().mtime = backBuffer().ctime;

  swapBuffers();

  requestNotes_ = nullptr;
  state_ = Active;

  auto pendingRequests = std::move(interests_);
  size_t i = 0;
  (void)i;
  for (auto rn : pendingRequests) {
    TRACE(requestNotes_, 3, "commit: deliver to pending request $0", ++i);
    rn->request->post([=]() { deliver(rn); });
  }
}

UnixTime ObjectCache::ConcreteObject::ctime() const {
  return frontBuffer().ctime;
}

const std::string& ObjectCache::ConcreteObject::varyingHeader(
    const BufferRef& name) const {
  for (const auto& header : varyingHeaders())
    if (iequals(name, header.first)) return header.second;

  static const std::string none;
  return none;
}

bool ObjectCache::ConcreteObject::update(RequestNotes* rn) {
  TRACE(rn, 3, "ConcreteObject.update() -> $0", state_);

  if (state_ != Spawning) state_ = Updating;

  if (!requestNotes_) {
    // this is the first interest request, so it must be responsible for
    // updating this object, too.
    requestNotes_ = rn;

    // avoid caching conditional GET by removing conditional request headers
    // when sending to backend.
    if (rn->request->method == "GET")
      rn->request->removeRequestHeaders({"If-Match", "If-None-Match",
                                         "If-Modified-Since",
                                         "If-Unmodified-Since"});

    rn->request->onPostProcess
        .connect<ConcreteObject, &ConcreteObject::postProcess>(this);
    return false;
  } else {
    // we have already some request that's updating this object, so add us to
    // the interest list
    // and wait for the response.
    interests_.push_back(rn);  // TODO honor updateLockTimeout
    TRACE(rn, 3, "Concurrent update detected. Enqueuing interest ($0).",
          interests_.size());
    return true;
  }
}

void ObjectCache::ConcreteObject::deliver(RequestNotes* rn) {
  internalDeliver(rn);
}

void ObjectCache::ConcreteObject::internalDeliver(RequestNotes* rn) {
  HttpRequest* r = rn->request;

  ++frontBuffer().hits;

  TRACE(rn, 3, "ConcreteObject.deliver(): hit $0, state $1",
        frontBuffer().hits, state_);

  if (equals(r->method, "GET")) {
    HttpStatus status = tryProcessClientCache(rn);
    if (status != HttpStatus::Undefined) {
      r->status = status;

      if (!frontBuffer().etag.empty())
        r->responseHeaders.push_back("ETag", frontBuffer().etag);

      char buf[256];
      time_t mtime = frontBuffer().mtime.unixtime();
      if (struct tm* tm = std::gmtime(&mtime)) {
        if (std::strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", tm) != 0) {
          r->responseHeaders.push_back("Last-Modified", buf);
        }
      }

      addHeaders(r, true);

      r->finish();
      return;
    }
  }

  r->status = frontBuffer().status;

  for (auto header : frontBuffer().headers)
    r->responseHeaders.push_back(header.first, header.second);

  addHeaders(r, true);

  char slen[16];
  snprintf(slen, sizeof(slen), "%zu", frontBuffer().body.size());
  r->responseHeaders.overwrite("Content-Length", slen);

  if (!equals(r->method, "HEAD")) r->write<BufferRefSource>(frontBuffer().body);

  r->finish();
}

/**
 * Attempts to shortcut request-processing by using the client-side cache.
 */
HttpStatus ObjectCache::ConcreteObject::tryProcessClientCache(
    RequestNotes* rn) {
  HttpRequest* r = rn->request;
  TRACE(rn, 1, "tryProcessClientCache()");

  // If-None-Match
  do {
    BufferRef value = r->requestHeader("If-None-Match");
    TRACE(rn, 1, "tryProcessClientCache(): If-None-Match: '$0'", value);
    if (value.empty()) continue;

    // XXX: on these entities we probably don't need the token-list support
    TRACE(rn, 1, " - against etag: '$0'", frontBuffer().etag);
    if (value != frontBuffer().etag) continue;

    return HttpStatus::NotModified;
  } while (0);

  // If-Modified-Since
  do {
    BufferRef value = r->requestHeader("If-Modified-Since");
    TRACE(rn, 1, "tryProcessClientCache(): If-Modified-Since: '$0'", value);
    if (value.empty()) continue;

    UnixTime dt(value);
    if (!dt.valid()) continue;

    if (frontBuffer().mtime > dt) continue;

    return HttpStatus::NotModified;
  } while (0);

  // If-Match
  do {
    BufferRef value = r->requestHeader("If-Match");
    TRACE(rn, 1, "tryProcessClientCache(): If-Match: '$0'", value);
    if (value.empty()) continue;

    if (value == "*") continue;

    // XXX: on static files we probably don't need the token-list support
    if (value == frontBuffer().etag) continue;

    return HttpStatus::PreconditionFailed;
  } while (0);

  // If-Unmodified-Since
  do {
    BufferRef value = r->requestHeader("If-Unmodified-Since");
    TRACE(rn, 1, "tryProcessClientCache(): If-Unmodified-Since: '$0'", value);
    if (value.empty()) continue;

    UnixTime dt(value);
    if (!dt.valid()) continue;

    if (frontBuffer().mtime <= dt) continue;

    return HttpStatus::PreconditionFailed;
  } while (0);

  return HttpStatus::Undefined;
}

void ObjectCache::ConcreteObject::expire() { state_ = Stale; }

void ObjectCache::ConcreteObject::destroy() {
  // reschedule all pending interested clients on this object as we're going to
  // get destroyed.
  // the reschedule should not include the cache again.
  auto pendingRequests = std::move(interests_);
  for (auto rn : pendingRequests) {

#ifdef ENABLE_DIRECTOR_CACHE
    rn->cacheIgnore = true;
#endif

    object()->store()->director()->reschedule(rn);
  }

  object()->destroy(this);
  delete this;
}
// }}}
// {{{ ObjectCache::Object
ObjectCache::Object::Object(ObjectCache* store, const std::string& cacheKey)
    : store_(store), cacheKey_(cacheKey), requestHeaders_(), objects_() {}

ObjectCache::Object::~Object() {}

ObjectCache::ConcreteObject* ObjectCache::Object::select(
    const RequestNotes* rn) {
  for (const auto& object : objects_)
    if (object->isMatch(rn->request)) return object;

  // FIXME: we actually need a tiny an rw-lock to the object-list access (tbb?).
  // FIXME: ConcreteObject actually needs a ref to its Object parent, so it can
  // update their varying request headers.

  ConcreteObject* co = new ConcreteObject(this);
  objects_.push_back(co);
  return co;
}

bool ObjectCache::Object::update(RequestNotes* rn) {
  if (ConcreteObject* object = select(rn)) {
    return object->update(rn);
  }

  return false;
}

void ObjectCache::Object::deliver(RequestNotes* rn) {
  if (ConcreteObject* object = select(rn)) {
    object->deliver(rn);
  }
}

void ObjectCache::Object::expire() {
  for (ConcreteObject* object : objects_) {
    object->expire();
  }
}

void ObjectCache::Object::destroy(ConcreteObject* concreteObject) {
  // FIXME: locking to local concrete objects list

  auto i = std::find(objects_.begin(), objects_.end(), concreteObject);
  if (i != objects_.end()) {
    objects_.erase(i);

    ObjectMap::accessor accessor;
    if (store_->objects_.find(accessor, cacheKey_)) {
      store_->objects_.erase(accessor);
      delete this;
    }
  }
}
// }}}
// {{{ ObjectCache
ObjectCache::ObjectCache(Director* director)
    : director_(director),
      enabled_(false),
      deliverActive_(true),
      deliverShadow_(true),
      lockOnUpdate_(true),
      updateLockTimeout_(10_seconds),
      defaultKey_("%h%r%q"),
      defaultTTL_(20_seconds),  // Duration::Zero),
      defaultShadowTTL_(Duration::Zero),
      cacheHits_(0),
      cacheShadowHits_(0),
      cacheMisses_(0),
      cachePurges_(0),
      cacheExpiries_(0),
      objects_() {}

ObjectCache::~ObjectCache() {}

bool ObjectCache::find(
    const std::string& cacheKey,
    const std::function<void(ObjectCache::Object*)>& callback) {
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

bool ObjectCache::acquire(
    const std::string& cacheKey,
    const std::function<void(ObjectCache::Object*, bool)>& callback) {
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

bool ObjectCache::purge(const std::string& cacheKey) {
  bool result = false;
  ObjectMap::accessor accessor;

  if (objects_.find(accessor, cacheKey)) {
    ++cachePurges_;
    accessor->second->expire();
    result = true;
  }

  return result;
}

void ObjectCache::expireAll() {
  for (auto object : objects_) {
    ++cachePurges_;
    object.second->expire();
  }
}

void ObjectCache::purgeAll() {
  for (auto object : objects_) {
    delete object.second;
  }

  cachePurges_ += objects_.size();
  objects_.clear();
}

bool ObjectCache::deliverActive(RequestNotes* rn) {
  if (!deliverActive()) return false;

  bool processed = false;

  acquire(rn->cacheKey, [&](Object* someObject, bool created) {
    if (created) {
      // cache object did not exist and got just created for by this request
      // printf("Director.processCacheObject: object created\n");
      ++cacheMisses_;
      processed = someObject->update(rn);
    } else if (someObject) {
      ConcreteObject* object = someObject->select(rn);

      const UnixTime now = rn->request->connection.worker().now();
      const UnixTime expiry = object->ctime() + rn->cacheTTL;

      if (expiry < now) object->expire();

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
            // TRACE(rn, 3, "ConcreteObject.deliver(): deliver shadow object");
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

bool ObjectCache::deliverShadow(RequestNotes* rn) {
  if (deliverShadow()) {
    if (find(rn->cacheKey, [rn, this](Object* object) {
          if (object) {
            ++cacheShadowHits_;
            rn->request->responseHeaders.push_back("X-Director-Cache",
                                                   "shadow");
            object->deliver(rn);
          }
        })) {
      return true;
    }
  }

  return false;
}

void ObjectCache::writeJSON(JsonWriter& json) const {
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
ObjectCache::Builder::Builder(ConcreteObject* object) : object_(object) {}

Buffer ObjectCache::Builder::process(const BufferRef& chunk) {
  if (object_) {
    TRACE(object_->requestNotes_, 3, "ObjectCache.Builder.process(): $0 bytes",
          chunk.size());
    if (!chunk.empty()) {
      object_->backBuffer().body.push_back(chunk);
    }
  }

  return Buffer(chunk);
}
// }}}
