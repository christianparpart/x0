#include <x0/io/chain_filter.hpp>

namespace x0 {

buffer chain_filter::process(const buffer_ref& input)
{
	if (filters_.empty())
		return buffer(input);

	auto i = filters_.begin();
	auto e = filters_.end();
	buffer result((*i++)->process(input));

	while (i != e)
	{
#if 1 // !defined(NDEBUG)
		buffer tmp((*i++)->process(result));
		result = std::move(tmp);
#else
		result = (*i++)->process(result.ref());
#endif
	}

	return result;
}

} // namespace x0
