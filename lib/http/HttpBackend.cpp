#include <x0/http/HttpBackend.h>
#include <x0/http/HttpDirector.h>
#include <x0/io/BufferSource.h>


#if !defined(NDEBUG)
#	define TRACE(msg...) (this->Logging::debug(msg))
#else
#	define TRACE(msg...) do {} while (0)
#endif

namespace x0 {

HttpBackend::HttpBackend(HttpDirector* director, const std::string& name, size_t capacity) :
#ifndef NDEBUG
	Logging("HttpBackend/%s", name.c_str()),
#endif
	director_(director),
	name_(name),
	capacity_(capacity),
	load_(),
	role_(Role::Active),
	enabled_(true),
	healthMonitor_(director_->worker_)
{
	healthMonitor_.onStateChange([&](HttpHealthMonitor*) {
		director_->worker_->log(Severity::info, "Director '%s': backend '%s' is now %s.",
			director_->name().c_str(), name_.c_str(), healthMonitor_.state_str().c_str());

		if (healthMonitor_.isOnline()) {
			// try delivering a queued request
			director_->dequeueTo(this);
		}
	});
}

HttpBackend::~HttpBackend()
{
}

size_t HttpBackend::capacity() const
{
	return capacity_;
}

//virtual
std::string HttpBackend::str() const
{
	return "TODO";
}

size_t HttpBackend::writeJSON(Buffer& output) const
{
	static const std::string roleStr[] = { "Active", "Standby" };
	static const std::string boolStr[] = { "false", "true" };
	size_t offset = output.size();

	output
		<< "\"name\": \"" << name_ << "\", "
		<< "\"capacity\": " << capacity_ << ", "
		<< "\"enabled\": " << boolStr[enabled_] << ", "
		<< "\"role\": \"" << roleStr[static_cast<int>(role_)] << "\",\n     "
		<< "\"load\": " << load_ << ",\n     "
		<< "\"health\": " << healthMonitor_
		;

	return output.size() - offset;
}

void HttpBackend::setState(HttpHealthMonitor::State value)
{
	healthMonitor_.setState(value);
}

/**
 * Invoked internally a request has been fully processed.
 *
 * This decrements the load-statistics, and potentially
 * dequeues possibly enqueued requests to take over.
 */
void HttpBackend::release()
{
	--load_;
	director_->release(this);
}

// {{{ NullProxy
NullProxy::NullProxy(HttpDirector* director, const std::string& name, size_t capacity) :
	HttpBackend(director, name, capacity)
{
}

bool NullProxy::process(HttpRequest* r)
{
	r->status = HttpError::ServiceUnavailable;
	r->finish();
	return true;
}
// }}}

} // namespace x0
