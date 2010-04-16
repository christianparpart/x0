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
public:
	enum state
	{
		START,
		SIZE_SPEC,
		CR1,
		LF1,
		CONTENT_START,
		CONTENT,
		CR2,	// CR of a filled chunk
		LF2,	// LF of a filled chunk
		CR3,	// CR of last zero-chunk
		LF3,	// LF of last zero-chunk
		END
	};

public:
	chunked_decoder();

	void reset();
	inline chunked_decoder::state state() const;

	virtual buffer process(const buffer_ref& input, bool eof = false);

private:
	buffer buffer_;
	enum state state_;
	std::size_t size_;
};

// {{{ inlines
inline void chunked_decoder::reset()
{
	buffer_.clear();
	state_ = START;
	size_ = 0;
}

inline enum chunked_decoder::state chunked_decoder::state() const
{
	return state_;
}
// }}}

//@}

} // namespace x0

#endif
