#include <x0/io/chain_filter.hpp>

namespace x0 {

buffer chain_filter::process(const buffer_ref& input, bool eof)
{
	if (filters_.empty())
		return buffer(input);

	auto i = filters_.begin();
	auto e = filters_.end();
	buffer result((*i++)->process(input, eof));

	while (i != e)
		result = (*i++)->process(result.ref(), eof);

	return result;
}

} // namespace x0
