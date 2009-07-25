#include <x0/io_service_pool.hpp>

#include <boost/thread.hpp>
#include <boost/bind.hpp>

namespace x0 {

io_service_pool::io_service_pool() :
	services_(), works_(), next_(0)
{
}

void io_service_pool::setup(std::size_t num_threads)
{
	for (std::size_t i = 0; i < num_threads; ++i)
	{
		io_service_ptr service(new boost::asio::io_service);
		services_.push_back(service);

		work_ptr work(new boost::asio::io_service::work(*service));
		works_.push_back(work);
	}
}

void io_service_pool::run()
{
	std::vector<boost::shared_ptr<boost::thread> > threads;

	for (std::size_t i = 1; i < services_.size(); ++i)
	{
		boost::shared_ptr<boost::thread> thread(new boost::thread(
			boost::bind(&boost::asio::io_service::run, services_[i])));

		threads.push_back(thread);
	}

	services_[0]->run();

	for (std::size_t i = 0; i < threads.size(); ++i)
	{
		threads[i]->join();
	}
}

void io_service_pool::stop()
{
	for (std::size_t i = 0; i < services_.size(); ++i)
	{
		services_[i]->stop();
	}
}

} // namespace x0
