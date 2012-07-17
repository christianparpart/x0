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
	checkInterval_(2000), // ms
	state_(State::Online),
	enabled_(true),
	offlineTime_(0)
{
	TRACE("create");
}

HttpBackend::~HttpBackend()
{
	TRACE("destroy");
}

void HttpBackend::onCheckState(ev::timer&, int)
{
	TRACE("checking state");
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
	size_t offset = output.size();

	output
		<< "\"name\": \"" << name() << "\", "
		<< "\"load\": " << load() << ", "
		<< "\"capacity\": " << capacity() << ", "
		<< "\"enabled\": " << enabled() << ", "
		<< "\"total\": " << total()
		;

	return output.size() - offset;
}

void HttpBackend::hit()
{
	++total_;
	++director_->total_;
}

void HttpBackend::release()
{
	--active_;
	director_->put(this);
}

// ----------------------------------------------------------------
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

} // namespace x0
