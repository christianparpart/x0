#ifndef sw_x0_io_filter_hpp
#define sw_x0_io_filter_hpp 1

#include <x0/buffer.hpp>
#include <x0/source.hpp>
#include <x0/sink.hpp>
#include <x0/api.hpp>
#include <memory>

namespace x0 {

/** unidirectional data processor.
 *
 * a filter is a processor, that reads from a source, and passes 
 * the received data to the sink. this data may or may not be
 * transformed befor passing to the sink.
 *
 * \see source, sink
 */
class X0_API filter
{
public:
	/** processes given input data through this filter. */
	virtual buffer process(const buffer::view& input) = 0;

public:
	buffer operator()(const buffer::view& input)
	{
		return process(input);
	}

	/** pumps a single source chunk through this filter, storing the result into the sink.
	 *
	 * \param src the source to pull from.
	 * \param snk the sink to push the filtered data into.
	 *
	 * Think of a data-flow directed graph like: source -> filter -> sink.
	 */
	virtual bool once(source& src, sink& snk)
	{
		buffer sb;
		buffer::view chunk = src.pull(sb);
		if (!chunk)
			return false;

		buffer pb = process(chunk);
		if (chunk = snk.push(pb))
			while (chunk = snk.push(chunk))
				;

		return true;
	}

	/** pumps whole source through this filter into given sink.
	 *
	 * \param src the source to pull from.
	 * \param snk the sink to push the filtered data into.
	 *
	 * Think of a data-flow directed graph like: source -> filter -> sink.
	 */
	virtual void all(source& src, sink& snk)
	{
		buffer result;
		while (buffer::view chunk = src.pull(result))
		{
			buffer p(process(chunk));

			if (buffer::view v = snk.push(p))
				while (v = snk.push(v))
					;

			p = process(buffer::view());
			if (!p.empty())
			//if (p = process(buffer::view()))
				if (buffer::view v = snk.push(p))
					while (v = snk.push(v))
						;

//			result.clear();
		}
	}
};

typedef std::shared_ptr<filter> filter_ptr;

} // namespace x0

#endif
