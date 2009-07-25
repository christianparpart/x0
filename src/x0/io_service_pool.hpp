#ifndef sw_x0_io_service_pool_hpp
#define sw_x0_io_service_pool_hpp 1

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <vector>

namespace x0 {

class io_service_pool :
	private boost::noncopyable
{
private:
	typedef boost::shared_ptr<boost::asio::io_service> io_service_ptr;
	typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;

	std::vector<io_service_ptr> services_;
	std::vector<work_ptr> works_;
	std::size_t next_;

public:
	io_service_pool();

	void setup(std::size_t num_services);

	void run();

	void stop();

	boost::asio::io_service& get_service();
};

// {{{ inlines
inline boost::asio::io_service& io_service_pool::get_service()
{
	boost::asio::io_service& service = *services_[next_++];

	if (next_ == services_.size())
	{
		next_ = 0;
	}

	return service;
}
// }}}

} // namespace x0

#endif
