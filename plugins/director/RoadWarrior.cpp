#include "RoadWarrior.h"
#include "Backend.h"
#include "HttpBackend.h"
#include "FastCgiBackend.h"
#include <x0/http/HttpRequest.h>

RoadWarrior::RoadWarrior(x0::HttpWorker* worker) :
	BackendManager(worker, "__roadwarrior__"),
	backends_()
{
}

RoadWarrior::~RoadWarrior()
{
	for (auto backend: backends_) {
		delete backend.second;
	}
}

void RoadWarrior::handleRequest(x0::HttpRequest* r, const x0::SocketSpec& spec, Type type)
{
	Backend* backend;
	auto bi = backends_.find(spec);
	if (bi != backends_.end()) {
		backend = bi->second;
	} else {
		switch (type) {
			case HTTP:
				backend = new HttpBackend(this, spec.str(), spec, 0, false);
				break;
			case FCGI:
				backend = new FastCgiBackend(this, spec.str(), spec, 0, false);
				break;
			default:
				r->status = x0::HttpStatus::InternalServerError;
				r->finish();
				return;
		}
		backends_[spec] = backend;
	}

	SchedulerStatus result = backend->tryProcess(r);
	if (result != SchedulerStatus::Success) {
		r->status = x0::HttpStatus::ServiceUnavailable;
		r->finish();
	}
}

void RoadWarrior::reject(x0::HttpRequest* r)
{
	// this request couldn't be served by the backend, so finish it with a 503 (Service Unavailable).

	r->status = x0::HttpStatus::ServiceUnavailable;
	r->finish();
}

void RoadWarrior::release(Backend* backend)
{
	// The passed backend just finished serving a request, so we might now pass it a queued request,
	// in case we would support queuing (do we want that?).
}

void RoadWarrior::writeJSON(x0::JsonWriter& json) const
{
	json.beginObject(name());
	json.beginArray("members");
	for (auto backend: backends_) {
		json.value(*backend.second);
	}
	json.endArray();
	json.endObject();
}
