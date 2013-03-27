#pragma once

#include "SchedulerStatus.h"
#include <vector>
#include <memory>

namespace x0 {
	class HttpRequest;
}

class Backend;
class Scheduler;

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

