/* <flow/FlowBackend.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.io/projects/flow
 *
 * (c) 2010-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/flow/FlowBackend.h>

namespace x0 {

void flow_print(void* p, FlowArray& args, void* cx);
bool printValue(const FlowValue& value);

FlowBackend::FlowBackend()
{
	registerFunction("__print", FlowValue::VOID, &flow_print, NULL);
}

FlowBackend::~FlowBackend()
{
	for (auto i = callbacks_.begin(), e = callbacks_.end(); i != e; ++i)
		delete *i;

	callbacks_.clear();
}

void FlowBackend::import(const std::string& name, const std::string& path)
{
	// XXX you should override me in our subclass
}

bool FlowBackend::registerNative(Callback::Type type, const std::string& name, FlowValue::Type returnType,
	CallbackFunction callback, void *userdata)
{
	Callback *cb = new Callback(type, returnType, name, callback, userdata);

	callbacks_.push_back(cb);

	return true;
}

bool FlowBackend::registerHandler(const std::string& name, CallbackFunction callback, void *userdata)
{
	return registerNative(Callback::HANDLER, name, FlowValue::BOOLEAN, callback, userdata);
}

bool FlowBackend::registerFunction(const std::string& name, FlowValue::Type returnType, CallbackFunction callback, void *userdata)
{
	return registerNative(Callback::FUNCTION, name, returnType, callback, userdata);
}

bool FlowBackend::registerProperty(const std::string& name, FlowValue::Type returnType, CallbackFunction callback, void *userdata)
{
	return registerNative(Callback::PROPERTY, name, returnType, callback, userdata);
}

int FlowBackend::find(const std::string& name) const
{
	for (int i = 0, e = callbacks_.size(); i < e; ++i)
		if (callbacks_[i]->name == name)
			return i;

	return -1;
}

FlowBackend::Callback *FlowBackend::at(int i) const
{
	return callbacks_[i];
}

void FlowBackend::invoke(int id, int argc, FlowValue *argv, void *cx)
{
	return callbacks_[id]->invoke(argc, argv, cx);
}

bool FlowBackend::unregisterNative(const std::string& name)
{
	for (auto i = callbacks_.begin(), e = callbacks_.end(); i != e; ++i)
	{
		if ((*i)->name == name)
		{
			delete *i;
			callbacks_.erase(i);
			return true;
		}
	}
	return false;
}

FlowBackend::Callback::Type FlowBackend::callbackTypeOf(const std::string& name) const
{
	for (auto i = callbacks_.begin(), e = callbacks_.end(); i != e; ++i)
		if ((*i)->name == name)
			return (*i)->type;

	return Callback::UNKNOWN;
}

void flow_print(void* p, FlowArray& args, void* cx)
{
	for (int i = 1, e = args.size(); i != e; ++i) {
		if (i > 1)
			printf("\t");

		args[i].dump(false);
	}
	printf("\n");
}
// }}}

//#define X0_DEBUG_BACKEND_CALLBACK 1

extern "C" X0_API void flow_backend_callback(uint64_t iself, int id, void *cx, int argc, FlowValue *argv)
{
	FlowBackend *self = (FlowBackend *)iself;

#if X0_DEBUG_BACKEND_CALLBACK
	printf("> flow_backend_callback(self=%p, id=%d, cx=%p, argc=%d)...\n", self, id, cx, argc);
	for (int i = 1; i < argc; ++i) {
		printf("    [%d]: ", i);
		argv[i].dump(true);
	}
#endif

	self->invoke(id, argc, argv, cx);

#if X0_DEBUG_BACKEND_CALLBACK
	printf("< flow_backend_callback: returns.\n");
	printf("    [R]: ");
	argv[0].dump(true);
#endif
}

} // namespace x0
