#pragma once

#include "BackendManager.h"
#include <x0/SocketSpec.h>
#include <unordered_map>

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
	enum Type {
		HTTP = 1,
		FCGI = 2,
	};

public:
	explicit RoadWarrior(x0::HttpWorker* worker);
	~RoadWarrior();

	void handleRequest(x0::HttpRequest* r, const x0::SocketSpec& spec, Type type);

	virtual void reject(x0::HttpRequest* r);
	virtual void release(Backend* backend);

	void writeJSON(x0::JsonWriter& output) const;

private:
	std::unordered_map<x0::SocketSpec, Backend*> backends_;
};
