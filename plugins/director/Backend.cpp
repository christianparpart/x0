#include "Backend.h"
#include "Director.h"
#include "HealthMonitor.h"

#if !defined(NDEBUG)
#	define TRACE(msg...) (this->Logging::debug(msg))
#else
#	define TRACE(msg...) do {} while (0)
#endif

using namespace x0;

Backend::Backend(Director* director,
	const std::string& name, const SocketSpec& socketSpec, size_t capacity,
	HealthMonitor* healthMonitor) :
#ifndef NDEBUG
	Logging("Backend/%s", name.c_str()),
#endif
	director_(director),
	name_(name),
	capacity_(capacity),
	load_(),
	role_(Role::Active),
	enabled_(true),
	socketSpec_(socketSpec),
	healthMonitor_(healthMonitor)
{
	healthMonitor_->setStateChangeCallback([&](HealthMonitor*) {
		director_->worker_->log(Severity::info, "Director '%s': backend '%s' is now %s.",
			director_->name().c_str(), name_.c_str(), healthMonitor_->state_str().c_str());

		if (healthMonitor_->isOnline()) {
			// try delivering a queued request
			director_->dequeueTo(this);
		}
	});

	director_->link(this);
}

Backend::~Backend()
{
	director_->unlink(this);
	delete healthMonitor_;
}

size_t Backend::capacity() const
{
	return capacity_;
}

void Backend::setCapacity(size_t value)
{
	capacity_ = value;
}

const std::string& Backend::role_str() const
{
	static const std::string str[] = {
		"active",
		"standby",
		"backup",
		"terminate",
	};

	return str[static_cast<unsigned>(role_)];
}

size_t Backend::writeJSON(Buffer& out) const
{
	static const std::string boolStr[] = { "false", "true" };
	size_t offset = out.size();

	out << "\"name\": \"" << name_ << "\", "
		<< "\"capacity\": " << capacity_ << ", "
		<< "\"enabled\": " << boolStr[enabled_] << ", "
		<< "\"protocol\": \"" << protocol() << "\", "
		<< "\"role\": \"" << role_str() << "\",\n     "
		<< "\"load\": " << load_ << ",\n     "
		<< "\"health\": " << *healthMonitor_ << ",\n     ";

	if (socketSpec_.isInet()) {
		out << "\"hostname\": \"" << socketSpec_.ipaddr().str() << "\"";
		out << ", \"port\": " << socketSpec_.port();
	} else {
		out << "\"path\": \"" << socketSpec_.local() << "\"";
	}

	return out.size() - offset;
}

void Backend::updateHealthMonitor()
{
	// TODO healthMonitor_->setRequest(...);
	//
	// this is currently done in the child classes, such as HttpBackend,
	// but we might reconsider this when finalizing the FastCGI backend type.
	// The FastCGI backend type might want to fully message-parse the setRequest()'s
	// input value in order to encode it into the FastCGI protocol.
}

void Backend::setRole(Role value)
{
	director_->worker_->log(Severity::debug, "setRole(%d) (from %d)", value, role_);
	if (role_ != value) {
		director_->unlink(this);
		role_ = value;
		director_->link(this);

		if (role_ == Role::Terminate) {
			director_->worker_->post([this](){ tryTermination(); });
		}
	}
}

/**
 * \note MUST be invoked from within the director's worker thread.
 */
bool Backend::tryTermination()
{
	if (role_ != Role::Terminate)
		return false;

	healthMonitor_->stop();

	if (load().current() > 0)
		return false;

	delete this;
	return true;
}

void Backend::terminate()
{
	setRole(Role::Terminate);
}

void Backend::setState(HealthMonitor::State value)
{
	healthMonitor_->setState(value);
}

bool Backend::assign(HttpRequest* r)
{
	auto notes = r->customData<DirectorNotes>(director_);
	notes->backend = this;

	++load_;
	++director_->load_;

	return process(r);
}

/**
 * Invoked internally a request has been fully processed.
 *
 * This decrements the load-statistics, and potentially
 * dequeues possibly enqueued requests to take over.
 */
void Backend::release()
{
	--load_;
	director_->release(this);
}
