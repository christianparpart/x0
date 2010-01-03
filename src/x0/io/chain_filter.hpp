#ifndef sw_x0_io_chain_filter_hpp
#define sw_x0_io_chain_filter_hpp 1

#include <x0/io/filter.hpp>
#include <vector>
#include <memory>

namespace x0 {

class chain_filter :
	public filter
{
public:
	virtual buffer process(const buffer::view& input);

public:
	void push_back(std::shared_ptr<filter> f);

	std::size_t size() const;
	bool empty() const;

private:
	std::vector<std::shared_ptr<filter> > filters_;
};

inline void chain_filter::push_back(std::shared_ptr<filter> f)
{
	filters_.push_back(f);
}

inline std::size_t chain_filter::size() const
{
	return filters_.size();
}

inline bool chain_filter::size() const
{
	return filters_.empty();
}

} // namespace x0

#endif
