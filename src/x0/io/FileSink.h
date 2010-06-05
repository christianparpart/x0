#ifndef sw_x0_io_FileSink_hpp
#define sw_x0_io_FileSink_hpp 1

#include <x0/io/SystemSink.h>
#include <string>

namespace x0 {

//! \addtogroup io
//@{

/** file sink.
 */
class X0_API FileSink :
	public SystemSink
{
public:
	explicit FileSink(const std::string& filename);
	~FileSink();
};

//@}

} // namespace x0

#endif
