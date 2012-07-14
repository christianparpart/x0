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
size_t HttpBackend::load() const
{
	return 0; //! TODO
}

//virtual
std::string HttpBackend::str() const
{
	return "TODO";
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
