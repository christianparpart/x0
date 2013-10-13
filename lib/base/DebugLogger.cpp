#include <x0/DebugLogger.h>
#include <x0/Tokenizer.h>
#include <x0/Buffer.h>
#include <cstdlib>
#include <cstdarg>

namespace x0 {

DebugLogger::Instance::Instance(DebugLogger* logger, const std::string& tag) :
	logger_(logger),
	tag_(tag),
	enabled_(false),
	verbosity_(1),
	codes_(),
	pre_(),
	post_()
{
}

DebugLogger::Instance::~Instance()
{
}

void DebugLogger::Instance::enable()
{
	enabled_ = true;
}

void DebugLogger::Instance::disable()
{
	enabled_ = false;
}

void DebugLogger::Instance::setVerbosity(int value)
{
	verbosity_ = value;
}

void DebugLogger::Instance::setPreference(const std::string& value)
{
	static const struct {
		int value;
		const std::string name;
	} codes[] = {
		{ 1, "bold" },
		{ 2, "faint"} ,
		{ 3, "italic" },
		{ 4, "underline" },
		{ 5, "blink" },

		{ 11, "font1" },
		{ 12, "font2" },

		{ 30, "black" },
		{ 31, "red" },
		{ 32, "green" },
		{ 33, "yellow" },
		{ 34, "blue" },
		{ 35, "magenta" },
		{ 36, "cyan" },
		{ 37, "white" },

		{ 40, "bg-black" },
		{ 41, "bg-red" },
		{ 42, "bg-green" },
		{ 43, "bg-yellow" },
		{ 44, "bg-blue" },
		{ 45, "bg-magenta" },
		{ 46, "bg-cyan" },
		{ 47, "bg-white" },
	};

	bool found = false;
	for (const auto& code: codes) {
		if (value == code.name) {
			codes_.push_back(code.value);
			found = true;
			break;
		}
	}

	if (!found)
		return;

	// update ansii code strings
	Buffer buf;
	buf.push_back("\033[");
	for (size_t i = 0; i < codes_.size(); ++i) {
		if (likely(i))
			buf.push_back(';');

		buf.push_back(codes_[i]);
	}
	buf.push_back("m");
	pre_ += buf.str();

	if (post_.empty())
		post_ = "\033[0m";
}

void DebugLogger::Instance::log(int level, const char* fmt, ...)
{
	if (!enabled_)
		return;

	if (level > verbosity_)
		return;

	char msg[1024];
	va_list va;
	va_start(va, fmt);
	vsnprintf(msg, sizeof(msg), fmt, va);
	va_end(va);

	if (logger_->colored()) {
		Buffer buf;
		buf.printf("%s[%s:%d] %s%s", pre_.c_str(), tag_.c_str(), level, msg, post_.c_str());
		logger_->onLogWrite(buf.c_str(), buf.size());
	} else {
		Buffer buf;
		buf.printf("[%s:%d] %s", tag_.c_str(), level, msg);
		logger_->onLogWrite(buf.c_str(), buf.size());
	}
}

void DebugLogger::Instance::logUntagged(int level, const char* fmt, ...)
{
	if (!enabled_)
		return;

	if (level > verbosity_)
		return;

	char msg[1024];
	va_list va;
	va_start(va, fmt);
	ssize_t n = vsnprintf(msg, sizeof(msg), fmt, va);
	va_end(va);

	if (logger_->colored()) {
		Buffer buf;
		buf.push_back(pre_);
		buf.push_back(msg, n);
		buf.push_back(post_);
		logger_->onLogWrite(buf.c_str(), buf.size());
	} else {
		logger_->onLogWrite(msg, n);
	}
}

void logWriteDefault(const char* msg, size_t n)
{
	printf("%s\n", msg);
}

DebugLogger::DebugLogger() :
	onLogWrite(std::bind(logWriteDefault, std::placeholders::_1, std::placeholders::_2)),
	configured_(false),
	map_(),
	colored_(true)
{
}

DebugLogger::~DebugLogger()
{
	reset();
}

// tagList ::= [tagSpec (':' tagSpec)*]
// tagSpec ::= TOKEN ('/' (VERBOSITY | COLOR))*
//
// "worker/3:director/10:connection:request/2"
// "worker/3/red:director/3/bight/green"
//
void DebugLogger::configure(const char* envvar)
{
	if (!envvar || !*envvar)
		return;

	typedef Tokenizer<BufferRef> Tokenizer;
	const char* envp = std::getenv(envvar);

	if (!envp || !*envp)
		return;

	Buffer buf(envp);
	auto tags = Tokenizer::tokenize(buf.ref(), ":");
	for (const auto& tagSpec: tags) {
		auto i = tagSpec.find('/');
		if (i == Buffer::npos) {
			enable(tagSpec.str().c_str());
		} else { // tag
			auto ref = tagSpec.ref(0, i);
			auto str = ref.str();
			auto instance = get(str.c_str());
			instance->enable();

			auto prefs = Tokenizer::tokenize(tagSpec.ref(i + 1), "/");
			for (const auto& pref: prefs) {
				if (!std::isdigit(pref[0])) {
					instance->setPreference(pref.str());
				} else {
					instance->setVerbosity(pref.toInt());
				}
			}
		}
	}

	configured_ = true;
}

void DebugLogger::reset()
{
	configured_ = false;
	onLogWrite = std::bind(logWriteDefault, std::placeholders::_1, std::placeholders::_2),
	each([&](Instance* in) { delete in; });
	map_.clear();
}

DebugLogger& DebugLogger::get()
{
	static DebugLogger logger;
	return logger;
}

void DebugLogger::enable(const std::string& tag)
{
	get(tag)->enable();
}

void DebugLogger::disable(const std::string& tag)
{
	get(tag)->disable();
}

void DebugLogger::enable()
{
	each([&](Instance* dl) { dl->enable(); });
}

void DebugLogger::disable()
{
	each([&](Instance* dl) { dl->disable(); });
}

} // namespace x0
