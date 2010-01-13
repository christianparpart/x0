#include <x0/chain_filter.hpp>

namespace x0 {

buffer chain_filter::process(const buffer_ref& input)
{
	if (filters_.empty())
		return buffer(input);

	buffer b(filters_[0]->process(input));

	for (std::size_t i = 1; i < filters_.size(); ++i)
		b = filters_[i]->process(b);

	return b;
}

} // namespace x0
