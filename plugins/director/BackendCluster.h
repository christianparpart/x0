#pragma once
/* <plugins/director/BackendCluster.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include "SchedulerStatus.h"
#include "Scheduler.h"
#include <vector>
#include <memory>

namespace x0 {
	class HttpRequest;
}

class Backend;
class Scheduler;
class RequestNotes;

class BackendCluster
{
public:
	typedef std::vector<Backend*> List;

	BackendCluster();
	~BackendCluster();

	template<typename T>
	void setScheduler() {
		delete scheduler_;
		scheduler_ = new T(&cluster_);
	}
	Scheduler* scheduler() const { return scheduler_; }

	SchedulerStatus schedule(x0::HttpRequest* r);

	bool empty() const { return cluster_.empty(); }
	size_t size() const { return cluster_.size(); }
	size_t capacity() const;

	List::const_iterator begin() const { return cluster_.begin(); }
	List::const_iterator end() const { return cluster_.end(); }
	List::const_iterator cbegin() const { return cluster_.cbegin(); }
	List::const_iterator cend() const { return cluster_.cend(); }

	Backend* front() const { return cluster_.front(); }
	Backend* back() const { return cluster_.back(); }

	void push_back(Backend* backend);
	void remove(Backend* backend);

	List& cluster() { return cluster_; }
	const List& cluster() const { return cluster_; }

protected:
	List cluster_;
	Scheduler* scheduler_;
};

