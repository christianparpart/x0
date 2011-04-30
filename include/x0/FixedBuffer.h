#ifndef sw_x0_FixedBuffer_h
#define sw_x0_FixedBuffer_h 1

#include <x0/Buffer.h>

namespace x0 {

template<const std::size_t N>
class FixedBuffer : public Buffer
{
private:
	value_type fixed_[N];

public:
	FixedBuffer();
	~FixedBuffer();

	virtual bool setCapacity(std::size_t value);
};

// {{{ FixedBuffer impl
template<std::size_t N>
inline FixedBuffer<N>::FixedBuffer() :
	Buffer()
{
	data_ = fixed_;
	size_ = 0;
	capacity_ = N;
}

template<std::size_t N>
FixedBuffer<N>::~FixedBuffer()
{
	// the parent's destructor will invoked setCapacity(0), however, they
	// will NOT use the overridden one, so we fake him a capacity of 0
	capacity_ = 0;
}

template<std::size_t N>
bool FixedBuffer<N>::setCapacity(std::size_t value)
{
	if (value > N)
		return false;

	capacity_ = value;
	return true;
}
// }}}

} // namespace x0

#endif
