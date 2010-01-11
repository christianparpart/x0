#ifndef sw_x0_io_pump_hpp
#define sw_x0_io_pump_hpp 1

#include <x0/source.hpp>
#include <x0/sink.hpp>
#include <x0/filter_source.hpp>
#include <x0/chain_filter.hpp>

namespace x0 {

//! \addtogroup io
//@{

void pump(x0::source& input, x0::sink& output);
void pump(x0::source& input, x0::sink& output, x0::filter& f);
void pump(x0::source& input, x0::sink& output, x0::chain_filter& cf);

// {{{ inline impl
inline void pump(x0::source& input, x0::sink& output)
{
	while (input.pump(output) > 0)
		;
}

inline void pump(x0::source& input, x0::sink& output, x0::filter& f)
{
	filter_source fs(input, f);
	pump(fs, output);
}

inline void pump(x0::source& input, x0::sink& output, x0::chain_filter& cf)
{
	if (cf.empty())
		pump(input, output);
	else
	{
		filter_source fs(input, cf);
		pump(fs, output);
	}
}
// }}}

//@}

} // namespace x0

#endif
