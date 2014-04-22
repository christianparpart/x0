#pragma once
/* <plugins/director/BackendCluster.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include "SchedulerStatus.h"
#include "Scheduler.h"
#include <functional>
#include <vector>
#include <memory>

class Backend;
class Scheduler;
struct RequestNotes;

/**
 * Manages a set of backends of one role.
 *
 * \see BackendRole
 * \see Director
 */
class BackendCluster
{
public:
    typedef std::vector<Backend*> List;

    BackendCluster();
    ~BackendCluster();

    template<typename T>
    void setScheduler() {
        auto old = scheduler_;
        scheduler_ = new T(&cluster_);
        delete old;
    }
    Scheduler* scheduler() const { return scheduler_; }

    SchedulerStatus schedule(RequestNotes* rn);

    bool empty() const { return cluster_.empty(); }
    size_t size() const { return cluster_.size(); }
    size_t capacity() const;

    void push_back(Backend* backend);
    void remove(Backend* backend);

    void each(const std::function<void(Backend*)>& cb);
    void each(const std::function<void(const Backend*)>& cb) const;
    bool find(const std::string& name, const std::function<void(Backend*)>& cb);
    Backend* find(const std::string& name);

protected:
    List cluster_;
    Scheduler* scheduler_;
};

