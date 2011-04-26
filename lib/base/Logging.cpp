#include <x0/Logging.h>
#include <sd-daemon.h>
#include <typeinfo>
#include <cstdarg>
#include <cstdio>

namespace x0 {

Logging::Logging() :
	prefix_(),
	className_(),
	enabled_(false)
{
}

Logging::Logging(const char *prefix, ...) :
	prefix_(),
	className_(),
	enabled_(false)
{
	char buf[1024];
	va_list va;

	va_start(va, prefix);
	vsnprintf(buf, sizeof(buf), prefix, va);
	va_end(va);

	prefix_ = buf;

	updateClassName();
}

void Logging::updateClassName()
{
	static const char splits[] = "/[(-";
	std::size_t pos;

	for (const char *split = splits; *split; ++split) {
		if ((pos = prefix_.find(*split)) != std::string::npos) {
			className_ = prefix_.substr(0, pos);
			return;
		}
	}

	className_ = prefix_;
}

bool Logging::checkEnabled()
{
	const char* env = getenv("XZERO_DEBUG");

	// TODO do a simple substring match for now but tokenize-split at ':' later
	return enabled_ || (env && strstr(env, className_.c_str()));
}

void Logging::setLoggingPrefix(const char *prefix, ...)
{
	char buf[1024];
	va_list va;

	va_start(va, prefix);
	vsnprintf(buf, sizeof(buf), prefix, va);
	va_end(va);

	prefix_ = buf;

	updateClassName();
}

void Logging::setLogging(bool enable)
{
	enabled_ = enable;
}

void Logging::debug(const char *fmt, ...)
{
	if (!checkEnabled())
		return;

	char buf[1024];
	va_list va;

	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	fprintf(stdout, SD_DEBUG "%s: %s\n", prefix_.c_str(), buf);
	fflush(stdout);
}

} // namespace x0
