#ifndef sw_x0_io_pump_hpp
#define sw_x0_io_pump_hpp 1

#include <x0/io/source.hpp>
#include <x0/io/sink.hpp>
#include <x0/io/chain_filter.hpp>

namespace x0 {

void pump(x0::source& input, x0::sink& output);
void pump(x0::source& input, x0::sink& output, x0::filter& f);
void pump(x0::source& input, x0::sink& output, x0::chain_filter& cf);

// {{{ inline impl
inline void pump(x0::source& input, x0::sink& output)
{
	x0::buffer buf;

	while (x0::buffer::view chunk = input.pull(buf))
	{
		while (chunk = output.push(chunk))
			;
	}
}

inline void pump(x0::source& input, x0::sink& output, x0::filter& f)
{
	f.all(input, output);
}

inline void pump(x0::source& input, x0::sink& output, x0::chain_filter& cf)
{
	if (cf.empty())
		pump(input, output);
	else
		cf.all(input, output);
}
// }}}

} // namespace x0

#endif
