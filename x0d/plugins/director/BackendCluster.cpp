/* <plugins/director/BackendCluster.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include "BackendCluster.h"
#include "Scheduler.h"
#include "Backend.h"

BackendCluster::BackendCluster() :
    cluster_(),
    scheduler_()
{
    // install default scheduler (round-robin)
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

SchedulerStatus BackendCluster::schedule(RequestNotes* rn)
{
    return scheduler_->schedule(rn);
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

/*!
 * Traverses all backends for read/write.
 */
void BackendCluster::each(const std::function<void(Backend*)>& cb)
{
    for (auto& item: cluster_) {
        cb(item);
    }
}

/*!
 * Traverses all backends for read-only.
 */
void BackendCluster::each(const std::function<void(const Backend*)>& cb) const
{
    for (const auto& item: cluster_) {
        cb(item);
    }
}

bool BackendCluster::find(const std::string& name, const std::function<void(Backend*)>& cb)
{
    for (const auto& item: cluster_) {
        if (item->name() == name) {
            cb(item);
            return true;
        }
    }

    return false;
}

Backend* BackendCluster::find(const std::string& name)
{
    for (const auto& item: cluster_) {
        if (item->name() == name) {
            return item;
        }
    }

    return nullptr;
}
