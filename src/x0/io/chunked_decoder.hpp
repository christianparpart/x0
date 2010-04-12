#ifndef sw_x0_io_chunked_decoder_hpp
#define sw_x0_io_chunked_decoder_hpp 1

#include <x0/buffer.hpp>
#include <x0/io/filter.hpp>
#include <x0/api.hpp>

namespace x0 {

//! \addtogroup io
//@{

/** "Chunked Encoding" decoder
 *
 * \see source, sink, chunked_encoder
 */
class X0_API chunked_decoder :
	public filter
{
private:
	enum state
	{
		START,
		SIZE_SPEC,
		CR1,
		LF1,
		CONTENT,
		CR2,
		LF2
	};

public:
	chunked_decoder();

	virtual buffer process(const buffer_ref& input, bool eof = false);

private:
	buffer buffer_;
	state state_;
	std::size_t size_;
};

//@}

} // namespace x0

#endif
