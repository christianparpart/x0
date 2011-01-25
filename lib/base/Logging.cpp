#include <x0/Logging.h>
#include <sd-daemon.h>
#include <cstdarg>
#include <cstdio>

namespace x0 {

Logging::Logging() :
	prefix_(),
	enabled_(true)
{
	setLoggingPrefix("unknown(%p)", (void *)this);
}

Logging::Logging(const char *prefix, ...) :
	prefix_(),
	enabled_(true)
{
	char buf[1024];
	va_list va;

	va_start(va, prefix);
	vsnprintf(buf, sizeof(buf), prefix, va);
	va_end(va);

	prefix_ = buf;
}

void Logging::setLoggingPrefix(const char *prefix, ...)
{
	char buf[1024];
	va_list va;

	va_start(va, prefix);
	vsnprintf(buf, sizeof(buf), prefix, va);
	va_end(va);

	prefix_ = buf;
}

void Logging::debug(bool enable)
{
	enabled_ = enable;
}

void Logging::debug(const char *fmt, ...)
{
	if (!enabled_)
		return;

	char buf[1024];
	va_list va;

	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	printf(SD_DEBUG "%s: %s\n", prefix_.c_str(), buf);
}

} // namespace x0
