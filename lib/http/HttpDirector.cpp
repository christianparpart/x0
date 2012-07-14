#include <x0/http/HttpDirector.h>
#include <x0/http/HttpBackend.h>
#include <x0/io/BufferSource.h>
#include <x0/Url.h>

#if !defined(NDEBUG)
#	define TRACE(msg...) (this->Logging::debug(msg))
#else
#	define TRACE(msg...) do {} while (0)
#endif

namespace x0 {

struct HttpDirectorNotes :
	public CustomData
{
	size_t retryCount;

	HttpDirectorNotes() :
		retryCount(0)
	{}
};

HttpDirector::HttpDirector(const std::string& name) :
#ifndef NDEBUG
	Logging("HttpDirector/%s", name.c_str()),
#endif
	name_(name),
	backends_(),
	lastBackend_(0),
	cloakOrigin_(true),
	maxRetryCount_(3)
{
}

HttpDirector::~HttpDirector()
{
}

size_t HttpDirector::capacity() const
{
	size_t result = 0;

	for (auto b: backends_)
		result += b->capacity();

	return result;
}

size_t HttpDirector::load() const
{
	size_t result = 0;

	for (auto b: backends_)
		result += b->load();

	return result;
}

HttpBackend* HttpDirector::createBackend(const std::string& name, const std::string& url)
{
	std::string protocol, hostname, path, query;
	int port = 0;

	if (!parseUrl(url, protocol, hostname, port, path, query)) {
		TRACE("invalid URL: %s", url.c_str());
		return false;
	}

	return createBackend(name, protocol, hostname, port, path, query);
}

HttpBackend* HttpDirector::createBackend(const std::string& name, const std::string& protocol,
	const std::string& hostname, int port, const std::string& path, const std::string& query)
{
	int capacity = 1;

	//TODO createBackend<HttpProxy>(hostname, port);
	if (protocol == "http")
		return createBackend<HttpProxy>(name, capacity, hostname, port);

	return nullptr;
}

void HttpDirector::enqueue(HttpRequest* r)
{
	r->responseHeaders.push_back("X-Director-Cluster", name_);

	r->setCustomData<HttpDirectorNotes>(this);

	for (HttpBackend* backend = selectBackend(r); backend; backend = nextBackend(backend, r))
		if (backend->process(r))
			return;

	// TODO enqueue to pendings-fifo
	r->status = HttpError::ServiceUnavailable;
	r->finish();
}

bool HttpDirector::requeue(HttpRequest* r, HttpBackend* backend)
{
	auto notes = r->customData<HttpDirectorNotes>(this);

	++notes->retryCount;

	TRACE("requeue (retry-count: %zi / %zi)", notes->retryCount, maxRetryCount());

	if (notes->retryCount < maxRetryCount())
		for (backend = nextBackend(backend, r); backend != nullptr; backend = nextBackend(backend, r))
			if (backend->process(r))
				return true;

	TRACE("requeue (retry-count: %zi / %zi): giving up", notes->retryCount, maxRetryCount());

	r->status = HttpError::ServiceUnavailable;
	r->finish();

	return false;
}

HttpBackend* HttpDirector::selectBackend(HttpRequest* r)
{
	HttpBackend* best = nullptr;
	size_t bestAvail = 0;

	for (HttpBackend* backend: backends_) {
		if (!backend->enabled()) {
			TRACE("selectBackend: skip %s (disabled)", backend->name().c_str());
			continue;
		}

		size_t l = backend->load();
		size_t c = backend->capacity();
		size_t avail = c - l;

		TRACE("selectBackend: test %s (%zi/%zi, %zi)", backend->name().c_str(), l, c, avail);

		if (avail > bestAvail) {
			TRACE(" - select");
			bestAvail = avail;
			best = backend;
		}
	}

	if (bestAvail > 0) {
		r->log(Severity::debug, "selecting backend %s", best->name().c_str());
		return best;
	} else {
		r->log(Severity::debug, "backend select failed. overloaded.");
		return nullptr;
	}
}

HttpBackend* HttpDirector::nextBackend(HttpBackend* backend, HttpRequest* r)
{
	auto i = std::find(backends_.begin(), backends_.end(), backend);
	if (i != backends_.end()) {
		auto k = i;
		++k;

		for (; k != backends_.end(); ++k) {
			if ((*k)->load() < (*k)->capacity()) {
				return *k;
			}
		}

		for (k = backends_.begin(); k != i; ++k) {
			if ((*k)->load() < (*k)->capacity()) {
				return *k;
			}
		}
	}

	TRACE("nextBackend: no next backend chosen.");
	return nullptr;
}

} // namespace x0
