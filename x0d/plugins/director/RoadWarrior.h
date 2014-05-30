#pragma once

#include "BackendManager.h"
#include <x0/SocketSpec.h>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace x0 {
    class JsonWriter;
}

class Backend;

/*!
 * Very basic backend-manager, used for simple reverse proxying of HTTP and FastCGI requests.
 */
class RoadWarrior : public BackendManager
{
public:
    enum Protocol {
        HTTP = 1,
        FCGI = 2,
    };

public:
    explicit RoadWarrior(x0::HttpWorker* worker);
    ~RoadWarrior();

    void handleRequest(RequestNotes* rn, const x0::SocketSpec& spec, Protocol protocol);

    void reject(RequestNotes* rn, x0::HttpStatus status) override;
    void release(RequestNotes* rn) override;

    void writeJSON(x0::JsonWriter& output) const;

private:
    Backend* acquireBackend(const x0::SocketSpec& spec, Protocol protocol);

private:
    std::mutex backendsLock_;
    std::unordered_map<x0::SocketSpec, std::unique_ptr<Backend>> backends_;
};
