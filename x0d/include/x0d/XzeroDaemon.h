#pragma once

#include <x0d/XzeroState.h>
#include <x0/sysconfig.h>
#include <x0/Api.h>
#include <x0/Buffer.h>
#include <x0/Severity.h>
#include <x0/Library.h>

#include <x0/flow/FlowBackend.h>
#include <x0/flow/FlowRunner.h>

#include <string>
#include <system_error>

namespace x0 {

class HttpServer;

class XzeroEventHandler;
class XzeroPlugin;
class XzeroCore;

class X0_API XzeroDaemon :
	public FlowBackend
{
private:
	friend class XzeroPlugin;
	friend class XzeroCore;

	/** concats a path with a filename and optionally inserts a path seperator if path 
	 *  doesn't contain a trailing path seperator. */
	static inline std::string pathcat(const std::string& path, const std::string& filename)
	{
		if (!path.empty() && path[path.size() - 1] != '/') {
			return path + "/" + filename;
		} else {
			return path + filename;
		}
	}

public:
	XzeroDaemon(int argc, char *argv[]);
	~XzeroDaemon();

	static XzeroDaemon *instance() { return instance_; }

	int run();

	void reexec();

	HttpServer* server() const { return server_; }

	void log(Severity severity, const char *msg, ...);

	void cycleLogs();

	// plugin management
	std::string pluginDirectory() const;
	void setPluginDirectory(const std::string& value);

	XzeroPlugin* loadPlugin(const std::string& name, std::error_code& ec);
	template<typename T> T* loadPlugin(const std::string& name, std::error_code& ec);
	void unloadPlugin(const std::string& name);
	std::vector<std::string> pluginsLoaded() const;

	XzeroPlugin* registerPlugin(XzeroPlugin* plugin);
	XzeroPlugin* unregisterPlugin(XzeroPlugin* plugin);

	XzeroCore& core() const;

	void addComponent(const std::string& value);

	bool setup(std::istream* settings, const std::string& filename = std::string(), int optimizationLevel = 2);
	bool setup(const std::string& filename, int optimizationLevel = 2);
	void dumpIR() const; // for debugging purpose

public: // FlowBackend overrides
	virtual void import(const std::string& name, const std::string& path);

	// setup
	bool registerSetupFunction(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata = nullptr);
	bool registerSetupProperty(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata = nullptr);

	// shared
	bool registerSharedFunction(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata = nullptr);
	bool registerSharedProperty(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata = nullptr);

	// main
	bool registerHandler(const std::string& name, CallbackFunction callback, void* userdata = nullptr);
	bool registerFunction(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata = nullptr);
	bool registerProperty(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void* userdata = nullptr);

private:
	bool validateConfig();
	bool createPidFile();
	bool parse();
	bool setupConfig();
	void daemonize();
	bool drop_privileges(const std::string& username, const std::string& groupname);

	void installCrashHandler();

private:
	XzeroState state_;
	int argc_;
	char** argv_;
	bool showGreeter_;
	std::string configfile_;
	std::string pidfile_;
	std::string user_;
	std::string group_;

	std::string logTarget_;
	std::string logFile_;
	Severity logLevel_;

	Buffer instant_;
	std::string documentRoot_;

	int nofork_;
	int systemd_;
	int doguard_;
	int dumpIR_;
	int optimizationLevel_;
	HttpServer *server_;
	unsigned evFlags_;
	XzeroEventHandler* eventHandler_;

	std::string pluginDirectory_;
	std::vector<XzeroPlugin*> plugins_;
	std::unordered_map<XzeroPlugin*, Library> pluginLibraries_;
	XzeroCore* core_;
	std::vector<std::string> components_;

	// flow configuration
	Unit* unit_;
	FlowRunner* runner_;
	std::vector<std::string> setupApi_;
	std::vector<std::string> mainApi_;

	static XzeroDaemon* instance_;
};

template<typename T>
inline T *XzeroDaemon::loadPlugin(const std::string& name, std::error_code& ec)
{
	return dynamic_cast<T*>(loadPlugin(name, ec));
}

inline XzeroCore& XzeroDaemon::core() const
{
	return *core_;
}

} // namespace x0
