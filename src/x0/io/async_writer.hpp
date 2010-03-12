#ifndef sw_x0_async_pump_hpp
#define sw_x0_async_pump_hpp 1

#include <x0/io/source.hpp>
#include <x0/io/connection_sink.hpp>
#include <x0/types.hpp>
#include <memory>

namespace x0 {

class connection;

//! \addtogroup io
//@{

void async_write(connection *target, const source_ptr& source, const completion_handler_type& handler);
void async_write(const std::shared_ptr<connection_sink>& snk, const source_ptr& source, const completion_handler_type& handler);

//@}

} // namespace x0

#endif
