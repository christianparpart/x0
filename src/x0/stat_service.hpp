#ifndef sw_x0_stat_service_hpp
#define sw_x0_stat_service_hpp (1)

#include <x0/cache.hpp>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace x0 {

/** service for retrieving file status.
 *
 * This is like stat(), in fact, it's using stat() but
 * caches the result for further use and also invalidates in realtime
 * in case their underlying inode has been updated.
 */
class stat_service :
	public boost::noncopyable
{
private:
	boost::asio::posix::stream_descriptor in_;	//!< inotify handle used for invalidating updated stat entries
	cache<std::string, struct stat> cache_;		//!< cache, storing path->stat pairs
	std::map<int, std::string> wd_;				//!< stores wd->path/stat pairs - if someone knows how to eliminate this extra map, tell me.
	inotify_event ev_;							//!< used to store a new inotify event

public:
	stat_service(boost::asio::io_service& io, std::size_t mxcost = 128);
	~stat_service();

	struct stat *query(const std::string& filename);
	struct stat *operator()(const std::string& filename);
	int operator()(const std::string& filename, struct stat *st);

	std::size_t size() const;
	bool empty() const;

	std::size_t cost() const;

	std::size_t max_cost() const;
	void max_cost(std::size_t value);

private:
	void invalidate(const boost::system::error_code& ec, std::size_t bytes_transferred)
	{
		printf("stat.invalidate(%s, %ld)\n", ec.message().c_str(), bytes_transferred);

//		if (!bytes_transferred)
//		{
//			::read(in_.native(), &ev_, sizeof(ev_)); // use read() instead of asio's std::readv()
//			perror("::read: ");
//		}

		{
			auto i = wd_.find(ev_.wd);

			if (i != wd_.end())
			{
				printf("state.invalidate: wd: %d, remove: %s\n", ev_.wd, i->second.c_str());
				cache_.remove(i->second);
				wd_.erase(i);
			}
		}

		if (!ec)
		{
			printf("stat: re-assign async_read (jobs=%ld)\n", size());
			boost::asio::async_read(in_, boost::asio::buffer(&ev_, sizeof(ev_)),
				boost::bind(&stat_service::invalidate, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		}
	}
};

inline stat_service::stat_service(boost::asio::io_service& io, std::size_t mxcost) :
	in_(io, ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC)),
	cache_(mxcost),
	wd_()
{
	boost::asio::async_read(in_, boost::asio::buffer(&ev_, sizeof(ev_)),
		boost::bind(&stat_service::invalidate, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

inline stat_service::~stat_service()
{
}

inline struct stat *stat_service::query(const std::string& _filename)
{
	std::string filename(_filename[_filename.size() - 1] == '/' ? _filename.substr(0, _filename.size() - 1) : _filename);
	if (struct stat *st = cache_(filename))
	{
		printf("stat.query.cached(%s)\n", filename.c_str());
		return st;
	}

	struct stat *st = new struct stat;

	if (::stat(filename.c_str(), st) == 0)
	{
		if (cache_(filename, st))
		{
			int rv = ::inotify_add_watch(in_.native(), filename.c_str(),
				IN_ONESHOT | IN_ATTRIB | IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT);

			if (rv != -1)
			{
				printf("stat.query(%s) wd=%d\n", filename.c_str(), rv);
				wd_[rv] = filename;
			}

			return st;
		}
	}
	printf("stat.query(%s) failed (%s)\n", filename.c_str(), strerror(errno));
	// either ::stat() or caching failed.

	delete st;

	return 0;
}

inline struct stat *stat_service::operator()(const std::string& filename)
{
	return query(filename);
}

inline int stat_service::operator()(const std::string& filename, struct stat *_st)
{
	if (struct stat *st = query(filename))
	{
		*_st = *st;
		return 0;
	}
	return -1;
}

inline std::size_t stat_service::size() const
{
	return cache_.size();
}

inline bool stat_service::empty() const
{
	return cache_.empty();
}

inline std::size_t stat_service::cost() const
{
	return cache_.cost();
}

inline std::size_t stat_service::max_cost() const
{
	return cache_.max_cost();
}

inline void stat_service::max_cost(std::size_t value)
{
	cache_.max_cost(value);
}

} // namespace x0

#endif
