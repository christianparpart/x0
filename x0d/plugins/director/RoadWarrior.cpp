#include "RoadWarrior.h"
#include "Backend.h"
#include "RequestNotes.h"
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

void RoadWarrior::handleRequest(RequestNotes* rn, const x0::SocketSpec& spec, Type type)
{
    auto r = rn->request;

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

    SchedulerStatus result = backend->tryProcess(rn);
    if (result != SchedulerStatus::Success) {
        r->status = x0::HttpStatus::ServiceUnavailable;
        r->finish();
    }
}

void RoadWarrior::reject(RequestNotes* rn)
{
    // this request couldn't be served by the backend, so finish it with a 503 (Service Unavailable).

    auto r = rn->request;
    r->status = x0::HttpStatus::ServiceUnavailable;
    r->finish();
}

void RoadWarrior::release(RequestNotes* rn)
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
