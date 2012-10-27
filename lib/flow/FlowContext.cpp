#include <x0/flow/FlowContext.h>

namespace x0 {

FlowContext::FlowContext() :
	regexMatch_(nullptr)
{
}

FlowContext::~FlowContext()
{
	if (regexMatch_ != nullptr)
		delete regexMatch_;
}

} // namespace x0
