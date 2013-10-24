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

} // namespace x0 
