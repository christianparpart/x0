#include <x0/flow2/FlowBackend.h>

namespace x0 {

FlowBackend::FlowBackend() :
	builtins_()
{
}

FlowBackend::~FlowBackend()
{
}

bool FlowBackend::contains(const std::string& name) const
{
	for (const auto& cb: builtins_)
		if (cb.name_ == name)
			return false;

	return true;
}

int FlowBackend::find(const std::string& name) const
{
	for (int i = 0, e = builtins_.size(); i != e; ++i)
		if (builtins_[i].name_ == name)
			return i;

	return -1;
}

FlowBackend::Callback& FlowBackend::registerHandler(const std::string& name)
{
	for (auto i = builtins_.begin(), e = builtins_.end(); i != e; ++i)
		if (i->name_ == name)
			return *i;

	builtins_.push_back(Callback::makeHandler(name));
	return builtins_[builtins_.size() - 1];
}

extern "C" X0_EXPORT void flow_native_call(FlowBackend* self, uint32_t id, FlowContext* cx, uint32_t argc, FlowValue* argv)
{
	printf("flow_native_call(self:%p, id:%d, cx:%p, argc:%d, argv:%p)\n", self, id, cx, argc, argv);
	FlowParams args(argc, argv);
	self->builtins()[id].invoke(args, cx);
}

} // namespace x0 
