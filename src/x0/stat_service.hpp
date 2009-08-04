#ifndef sw_x0_stat_service_hpp
#define sw_x0_stat_service_hpp (1)

#include <x0/cache.hpp>
#include <x0/api.hpp>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/signal.hpp>

#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#if 0
#	define STAT_DEBUG(msg...) printf("stat_service: " msg)
#else
#	define STAT_DEBUG(msg...) /*!*/
#endif

namespace x0 {

/** service for retrieving file status.
 *
 * This is like stat(), in fact, it's using stat() but
 * caches the result for further use and also invalidates in realtime
 * in case their underlying inode has been updated.
 *
 * \note this class is not thread-safe
 */
class X0_API stat_service :
	public boost::noncopyable
{
private:
	boost::asio::posix::stream_descriptor in_;	//!< inotify handle used for invalidating updated stat entries
	cache<std::string, struct stat> cache_;		//!< cache, storing path->stat pairs
	std::map<int, std::string> wd_;				//!< stores wd->path/stat pairs - if someone knows how to eliminate this extra map, tell me.
	boost::array<char, 8192> inbuf_;
	inotify_event ev_;							//!< used to store a new inotify event
	bool caching_;								//!< true if caching should be used, false otherwise
	struct stat st_;							//!< fallback stat struct when caching is disabled.

public:
	stat_service(boost::asio::io_service& io, std::size_t mxcost = 128);
	~stat_service();

	boost::signal<void(const std::string& filename, const struct stat *st)> on_invalidate;

	struct stat *query(const std::string& filename);
	struct stat *operator()(const std::string& filename);
	int operator()(const std::string& filename, struct stat *st);

	bool caching() const;
	void caching(bool);

	std::size_t size() const;
	bool empty() const;

	std::size_t cost() const;

	std::size_t max_cost() const;
	void max_cost(std::size_t value);

private:
	void async_read();

	void invalidate(const boost::system::error_code& ec, std::size_t bytes_transferred)
	{
		STAT_DEBUG("stat.invalidate(ec=%s, bt=%ld)\n", ec.message().c_str(), bytes_transferred);
		inotify_event *ev = reinterpret_cast<inotify_event *>(inbuf_.data());
		inotify_event *ee = ev + bytes_transferred;

		while (ev < ee && ev->wd != 0)
		{
			STAT_DEBUG("--stat.invalidate(len=%d, fn=%s)\n", ev->len, ev->name);
			auto i = wd_.find(ev->wd);

			if (i != wd_.end())
			{
				STAT_DEBUG("--stat.invalidate: wd: %d, remove: %s\n", ev->wd, i->second.c_str());

				auto k = cache_.find(i->second);
				on_invalidate(k->first, k->second.value_ptr);

				cache_.erase(k);
				wd_.erase(i);
			}
			ev += sizeof(*ev) + ev->len;
		}

		if (!ec)
		{
			async_read();
		}
	}
};

inline stat_service::stat_service(boost::asio::io_service& io, std::size_t mxcost) :
#if defined(X0_RELEASE)
	in_(io, ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC)),
#else
	in_(io, ::inotify_init()),
#endif
	cache_(mxcost),
	wd_(),
	caching_(true)
{
#if !defined(X0_RELEASE)
	::fcntl(in_.native(), F_SETFL, O_NONBLOCK);
	::fcntl(in_.native(), F_SETFD, FD_CLOEXEC);
#endif
	async_read();
}

inline void stat_service::async_read()
{
	STAT_DEBUG("stat: (re-)assign async_read (watches=%ld)\n", size());
	boost::asio::async_read(in_, boost::asio::buffer(inbuf_),
		boost::asio::transfer_at_least(sizeof(inotify_event)),
		boost::bind(&stat_service::invalidate, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

inline stat_service::~stat_service()
{
}

inline struct stat *stat_service::query(const std::string& _filename)
{
	if (!caching_)
	{
		if (::stat(_filename.c_str(), &st_) == 0)
			return &st_;

		return 0;
	}

	std::string filename(_filename[_filename.size() - 1] == '/' ? _filename.substr(0, _filename.size() - 1) : _filename);
	if (struct stat *st = cache_(filename))
	{
		STAT_DEBUG("stat.query.cached(%s)\n", filename.c_str());
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
				STAT_DEBUG("stat.query(%s) wd=%d\n", filename.c_str(), rv);
				wd_[rv] = filename;
			}

			return st;
		}
	}
	STAT_DEBUG("stat.query(%s) failed (%s)\n", filename.c_str(), strerror(errno));
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

inline bool stat_service::caching() const
{
	return caching_;
}

inline void stat_service::caching(bool value)
{
	caching_ = value;

	if (!caching_)
	{
		cache_.clear();
	}
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
