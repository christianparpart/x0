/* <x0/io_service_pool.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_io_service_pool_hpp
#define sw_x0_io_service_pool_hpp 1

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <vector>

namespace x0 {

/** manages a set of io_service instances to be used when running x0 multi threaded - ideally, each thread gets one io_service dedicated.
 *
 * When creating an x0-server instance, the number of threads decides how many io_services will be allocated.
 * Currently, when running N io_service instances, the x0-server will spawn (N minus 1) threads as the main thread is running 
 * one io_service aswell.
 *
 * Each io_service keeps running even when no actual jobs are queued as this manager object queued a stub-work into each
 * io_service in order to let them <b>only</b> stop when the user application wants it to stop.
 *
 * \see server
 */
class io_service_pool :
	private boost::noncopyable
{
private:
	typedef boost::shared_ptr<boost::asio::io_service> io_service_ptr;
	typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;

	/** a list of all io_service objects being spawned. */
	std::vector<io_service_ptr> services_;

	/** holds a list of the stub works to be assigned to the io_services in order to never run out of work. */
	std::vector<work_ptr> works_;

	/** represents the index to the next io_service to be returned by \p get_service() call. */
	std::size_t next_;

public:
	/** initialize an empty io_service pool. */
	io_service_pool();

	/** initializes this io_service pool with \p num_service io_service instances.
	 *
	 * \param num_services the number of io_service instances to spawn.
	 */
	void setup(std::size_t num_services);

	/** starts running the io_service main-loops for each io_service within its own thread but
	 *  keeps one io_service running in the callers thread.
	 */
	void run();

	/** stops all io_service instances currently running and returns once all have completed their jobs. */
	void stop();

	/** retrieves an io_service for the caller's thread.
	 *
	 * This is currently done by just cycling through all io_service instances being load and returning 
	 * the next after the last's call returned on each call.
	 */
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
