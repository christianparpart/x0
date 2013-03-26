#include "BackendCluster.h"
#include "Scheduler.h"
#include "Backend.h"

BackendCluster::BackendCluster() :
	cluster_(),
	scheduler_()
{
	setScheduler<RoundRobinScheduler>();
}

BackendCluster::~BackendCluster()
{
	delete scheduler_;
}

size_t BackendCluster::capacity() const
{
	size_t result = 0;

	for (auto backend: cluster_)
		result += backend->capacity();

	return result;
}

SchedulerStatus BackendCluster::schedule(x0::HttpRequest* r)
{
	return scheduler_->schedule(r);
}

void BackendCluster::push_back(Backend* backend)
{
	cluster_.push_back(backend);
}

void BackendCluster::remove(Backend* backend)
{
	auto i = std::find(cluster_.begin(), cluster_.end(), backend);

	if (i != cluster_.end()) {
		cluster_.erase(i);
	}
}

