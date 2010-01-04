#include <x0/io/chain_filter.hpp>

namespace x0 {

buffer chain_filter::process(const buffer::view& input)
{
	if (filters_.empty())
		return buffer(input);

	buffer b(filters_[0]->process(input));

	for (std::size_t i = 1; i < filters_.size(); ++i)
		b = filters_[i]->process(b);

	return b;
}

bool chain_filter::once(source& src, sink& snk)
{
	if (!filters_.empty())
		return filter::once(src, snk);

	buffer b;

	buffer::view v(src.pull(b));
	if (v.empty())
		return false;

	while (v = snk.push(v))
		;

	return true;
}

void chain_filter::all(source& src, sink& snk)
{
	if (!filters_.empty())
		return filter::all(src, snk);

	buffer b;
	while (buffer::view v = src.pull(b))
	{
		while (v = snk.push(v))
			;

		b.clear();
	}
}

std::shared_ptr<chain_filter> chain(std::shared_ptr<filter> a, std::shared_ptr<filter> b)
{
	std::shared_ptr<chain_filter> cf(new chain_filter());

	cf->push_back(a);
	cf->push_back(b);

	return cf;
}

} // namespace x0
