#pragma once

#include <x0/Buffer.h>
#include <x0/Api.h>

#ifndef __APPLE__
#include <unordered_map>
#include <vector>
#include <functional>

namespace x0 {

/**
 * Customizable, taggable debug logging.
 */
class X0_API DebugLogger
{
public:
	class X0_API Instance { // {{{
	public:
		Instance(DebugLogger* logger, const std::string& tag);
		~Instance();

		const std::string& tag() const { return tag_; }
		bool isEnabled() const { return enabled_; }
		void setEnabled(bool value) { enabled_ = value; }
		void enable() { enabled_ = true; }
		void disable() { enabled_ = false; }

		void setVerbosity(int value) { verbosity_ = value; }
		void setPreference(const std::string& value);

		void log(int level, const char* fmt, ...);
		void logUntagged(int level, const char* fmt, ...);

		template<typename... Args>
		void log(const char* fmt, Args&... args) { log(1, fmt, args...); }

	private:
		DebugLogger* logger_;
		std::string tag_;
		bool enabled_;
		int verbosity_;
		std::vector<int> codes_;
		std::string pre_;
		std::string post_;
	};
	friend class Instance;
	// }}}

	DebugLogger();
	~DebugLogger();

	void configure(const char* envvar);
	bool isConfigured() const { return configured_; }
	void reset();

	/**
	 * @param tag Classifies the debug message to group certain kind of events together.
	 * @param level Debug verbosity. The higher the number the more verbose the message.
	 * @param fmt the debug message format string (printf-style)
	 */
	template<typename... Args>
	void log(const std::string& tag, int level, const char* fmt, const Args&... args)
	{
		if (auto instance = get(tag)) {
			instance->log(level, fmt, args...);
		}
	}

	template<typename... Args>
	void logUntagged(const std::string& tag, int level, const char* fmt, const Args&... args)
	{
		if (auto instance = get(tag)) {
			instance->logUntagged(level, fmt, args...);
		}
	}

	/**
	 * Retrieves global singleton object.
	 */
	static DebugLogger& get();

	/**
	 * Retrieves a unique instance for debug-logging messages of given unique tag.
	 *
	 * @param tag the unique tag that classifies all debug messages being passed through this instance.
	 * @return a debug stream instance that is used for only these kind of debug messages.
	 */
	static Instance* get(const std::string& tag)
	{
		return get()[tag];
	}

	Instance* operator[](const std::string& tag)
	{
		auto i = map_.find(tag);
		if (i != map_.end())
			return i->second;

		auto logger = new Instance(this, tag);
		map_[tag] = logger;
		return logger;
	}

	/** 
	 * Iterates through each tag instance.
	 *
	 * @param yield a callback function that is invoked for each tag instance currently registered.
	 */
	template<typename T>
	void each(const T& yield) { for (auto& item: map_) yield(item.second); }

	/**
	 * Enables a given instance.
	 */
	void enable(const std::string& tag);

	/**
	 * Disables a given instance.
	 */
	void disable(const std::string& tag);

	/**
	 * Enables all currently available instances.
	 */
	void enable();

	/**
	 * Disables all currently available instances.
	 */
	void disable();

	bool colored() const { return colored_; }
	void setColored(bool value) { colored_ = value; }

	std::function<void(const char*, size_t)> onLogWrite;

private:
	bool configured_;
	std::unordered_map<std::string, Instance*> map_;
	bool colored_;
};

#if 1 //defined(X0_ENABLE_DEBUG)
#	define X0_DEBUG(tag, level, msg...) do {DebugLogger::get().log((tag), (level), msg); } while (0)
#else
#	define X0_DEBUG(tag, level, msg...) do { } while (0)
#endif

#define XZERO_DEBUG(tag, level, msg...) X0_DEBUG((tag), (level), msg)

} // namespace x0
#endif
