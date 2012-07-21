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
	active_(0),
	total_(0),
	role_(Role::Active),
	enabled_(true),
	healthMonitor_(director_->worker_)
{
	healthMonitor_.onStateChange([&](HttpHealthMonitor*) {
		if (healthMonitor_.isOnline()) {
			director_->worker_->log(Severity::info, "Director '%s': backend '%s' is online now.", director_->name().c_str(), name_.c_str());
			// try delivering a queued request
			director_->put(this);
		} else {
			director_->worker_->log(Severity::warn, "Director '%s': backend '%s' is offline now.", director_->name().c_str(), name_.c_str());
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
		<< "\"load\": " << load() << ", "
		<< "\"capacity\": " << capacity_ << ", "
		<< "\"enabled\": " << boolStr[enabled_] << ", "
		<< "\"role\": \"" << roleStr[static_cast<int>(role_)] << "\", "
		<< "\"state\": \"" << healthMonitor_.state_str() << "\", "
		<< "\"total\": " << total_
		;

	return output.size() - offset;
}

void HttpBackend::hit()
{
	++total_;
	++director_->total_;
}

void HttpBackend::setState(HttpHealthMonitor::State value)
{
	healthMonitor_.setState(value);
}

void HttpBackend::release()
{
	--active_;
	director_->put(this);
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
