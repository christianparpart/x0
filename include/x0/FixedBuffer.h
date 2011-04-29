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
bool FixedBuffer<N>::setCapacity(std::size_t value)
{
	if (value > N)
		return true;

	capacity_ = value;
	return false;
}
// }}}

} // namespace x0

#endif
