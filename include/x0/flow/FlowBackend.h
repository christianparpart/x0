/* <flow/FlowBackend.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.ws/projects/flow
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_Flow_Backend_h
#define sw_Flow_Backend_h (1)

#include <x0/flow/FlowValue.h>

#include <string>
#include <vector>
#include <functional>

namespace x0 {

class FlowValue;

class FlowBackend
{
public:
	typedef void (*CallbackFunction)(void * /*userdata*/, int /*argc*/, FlowValue * /*argv*/, void * /*context*/);

	struct Callback // {{{
	{
		enum Type { UNKNOWN, FUNCTION, HANDLER, VARIABLE } type;
		std::string name;
		void *userdata;
		void *context;
		CallbackFunction callback;

		FlowValue::Type returnType;

		Callback() :
			type(UNKNOWN), name(), userdata(NULL), context(NULL), callback(), returnType(FlowValue::VOID) {}

		Callback(Type t, FlowValue::Type rt, const std::string& n, CallbackFunction cb, void *u) :
			type(t), name(n), userdata(u), callback(cb), returnType(rt) {}

		void invoke(int argc, FlowValue *argv, void *cx) { callback(userdata, argc, argv, cx); }
	};
	//}}}

public:
	FlowBackend();
	~FlowBackend();

	virtual void import(const std::string& name, const std::string& path);

	void setErrorHandler(std::function<void(const std::string&)>&& callback);

	bool registerHandler(const std::string& name, CallbackFunction callback, void *userdata = NULL);
	bool registerFunction(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void *userdata = NULL);
	bool registerVariable(const std::string& name, const FlowValue::Type returnType, CallbackFunction callback, void *userdata = NULL);

	bool registerNative(Callback::Type type, const std::string& name, FlowValue::Type returnType,
		CallbackFunction callback, void *userdata = NULL);

	int find(const std::string& name) const;
	Callback *at(int id) const;
	void invoke(int id, int argc, FlowValue *argv, void *cx);

	bool unregisterNative(const std::string& name);

	Callback::Type callbackTypeOf(const std::string& name) const;
	bool isFunction(const std::string& name) const { return callbackTypeOf(name) == Callback::FUNCTION; }
	bool isHandler(const std::string& name) const { return callbackTypeOf(name) == Callback::HANDLER; }
	bool isCallable(const std::string& name) const { auto t = callbackTypeOf(name); return t == Callback::FUNCTION || t == Callback::HANDLER; }
	bool isVariable(const std::string& name) const { return callbackTypeOf(name) == Callback::VARIABLE; }

private:
	std::vector<Callback *> callbacks_;
};

extern "C" void flow_backend_callback(uint64_t self, int id, void *cx, int argc, FlowValue *argv);

} // namespace x0

#endif
