#ifndef sw_x0_async_pump_hpp
#define sw_x0_async_pump_hpp 1

#include <x0/io/Source.h>
#include <x0/io/SocketSink.h>
#include <x0/Types.h>
#include <memory>

namespace x0 {

//! \addtogroup io
//@{

void writeAsync(Socket *target, const SourcePtr& source, const CompletionHandlerType& completionHandler);
void writeAsync(const std::shared_ptr<SocketSink>& target, const SourcePtr& source, const CompletionHandlerType& completionHandler);

//@}

} // namespace x0

#endif
