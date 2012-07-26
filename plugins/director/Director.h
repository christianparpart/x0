#pragma once

#include <x0/Counter.h>
#include <x0/Logging.h>
#include <x0/http/HttpRequest.h>
#include <x0/CustomDataMgr.h>
#include <ev++.h>

using namespace x0;

class Backend;

struct DirectorNotes :
	public CustomData
{
	size_t retryCount;
	Backend* backend;

	DirectorNotes() :
		retryCount(0),
		backend(nullptr)
	{}
};

/*!
 * \brief Load balancing HTTP request proxy.
 *
 * A \c Director implements load balancing over multiple \c Backend
 * instances of different transport type.
 * It supports weights and multiple states, such as (online/offline)
 * and (active/standby).
 *
 * \todo thread safety for actual horizontal scalability.
 * \todo support requeuing requests when designated backend did not respond in time.
 */
class Director :
#ifndef NDEBUG
	public Logging
#endif
{
private:
	HttpWorker* worker_;

	//! directors name, as used for debugging and displaying.
	std::string name_;

	bool mutable_; //!< whether or not one may create/update/delete backends at runtime

	//! set of backends managed by this director.
	std::vector<Backend*> backends_;

	std::deque<HttpRequest*> queue_; //! list of queued requests.
	size_t queueLimit_;

	Counter load_;
	Counter queued_;

	//! last backend-index a request has been successfully served with
	size_t lastBackend_;

	//! whether or not to cloak origin "Server" response-header
	bool cloakOrigin_;

	//! number of attempts to pass request to a backend before giving up
	size_t maxRetryCount_;

	std::string storagePath_;

public:
	Director(HttpWorker* worker, const std::string& name);
	~Director();

	const std::string& name() const { return name_; }

	bool isMutable() const { return mutable_; }
	void setMutable(bool value) { mutable_ = value; }

	size_t capacity() const;

	const Counter& load() const { return load_; }
	const Counter& queued() const { return queued_; }

	size_t queueLimit() const { return queueLimit_; }
	void setQueueLimit(size_t value) { queueLimit_ = value; }

	const std::vector<Backend*>& backends() const { return backends_; }

	bool cloakOrigin() const { return cloakOrigin_; }
	void setCloakOrigin(bool value) { cloakOrigin_ = value; }

	size_t maxRetryCount() const { return maxRetryCount_; }
	void setMaxRetryCount(size_t value) { maxRetryCount_ = value; }

	Backend* createBackend(const std::string& name, const std::string& url);

	Backend* createBackend(const std::string& name, const std::string& protocol, const std::string& hostname,
		int port, const std::string& path, const std::string& query);

	template<typename T, typename... Args>
	Backend* createBackend(const std::string& name, size_t capacity, const Args&... args) {
		return new T(this, name, capacity, args...);
	}

	Backend* findBackend(const std::string& name);

	void schedule(HttpRequest* r);
	bool reschedule(HttpRequest* r, Backend* backend);

	HttpRequest* dequeue();
	void dequeueTo(Backend* backend);

	void writeJSON(x0::Buffer& output);

	bool load(const std::string& path);
	bool store(const std::string& path = "");

private:
	Backend* selectBackend(HttpRequest* r);
	Backend* nextBackend(Backend* backend, HttpRequest* r);
	void enqueue(HttpRequest* r);
	void release(Backend* backend);

	void onStop();

	friend class Backend;
};
