/* <plugins/director/ClassfulScheduler.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#pragma once

#include "Scheduler.h"
#include "Backend.h"
#include <deque>
#include <algorithm> // std::max()
#include <ev++.h>

/**
 * Classful Request Scheduler, based on the HTB (Hierarchical Token Bucket) algorithm.
 */
class ClassfulScheduler :
	public Scheduler
{
public:
	class Bucket;

private:
	Bucket* rootBucket_;

public:
	explicit ClassfulScheduler(Director* d);
	~ClassfulScheduler();

	Bucket* createBucket(const std::string& name, size_t rate, size_t ceil);
	Bucket* findBucket(const std::string& name) const;

	// Scheduler API overrides
	virtual void schedule(x0::HttpRequest* r);
	virtual void reschedule(x0::HttpRequest* r);

	virtual void dequeueTo(Backend* backend);

	virtual void writeJSON(x0::JsonWriter& json) const;
	virtual bool load(x0::IniFile& settings);
	virtual bool save(x0::Buffer& out);

private:
	Backend* findLeastLoad(Backend::Role role, bool* allDisabled = nullptr);
	void pass(x0::HttpRequest* r, RequestNotes* notes, Backend* backend);
};

class ClassfulScheduler::Bucket
#ifndef NDEBUG
	: public x0::Logging
#endif
{
private:
	ClassfulScheduler* scheduler_;
	Bucket* parent_;

	std::string name_;
	size_t rate_;
	size_t ceil_;
	size_t available_;
	std::vector<Bucket*> children_;

	x0::Counter load_;
	x0::Counter queued_;
	std::deque<x0::HttpRequest*> queue_;
	ev::timer queueTimer_;

	size_t dequeueOffset_;

public:
	Bucket(ClassfulScheduler* scheduler, Bucket* parent, const std::string& name, size_t rate, size_t ceil = 0);
	~Bucket();

	Director* director() const { return scheduler_->director(); }
	Scheduler* scheduler() const { return scheduler_; }
	const std::string& name() const { return name_; }

	size_t rate() const { return rate_; }
	size_t ceil() const { return ceil_; }

	void setRate(size_t value);
	void setCeil(size_t value);

	size_t available() const { return available_; }
	size_t actualRate() const { return ceil() - available(); }
	size_t overRate() const { return std::max(static_cast<int>(actualRate()) - static_cast<int>(rate()), 0); }

	Bucket* createChild(const std::string& name, size_t rate, size_t ceil);
	Bucket* findChild(const std::string& name) const;

	size_t get(size_t n = 1);
	void put(size_t n = 1);

	void enqueue(x0::HttpRequest* r);
	x0::HttpRequest* dequeue();

	const x0::Counter& load() const { return load_; }
	const x0::Counter& queued() const { return queued_; }

	void writeJSON(x0::JsonWriter& json) const;

private:
	void updateQueueTimer();
};

inline x0::JsonWriter& operator<<(x0::JsonWriter& json, const ClassfulScheduler::Bucket& value)
{
	value.writeJSON(json);
	return json;
}

