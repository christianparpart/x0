#ifndef sw_x0_cache_Redis_h
#define sw_x0_cache_Redis_h

#include <x0/cache/CacheService.h>
#include <x0/IPAddress.h>
#include <x0/Socket.h>
#include <x0/Buffer.h>

namespace x0 {

class X0_API Redis :
	public CacheService
{
public:
	explicit Redis(struct ev_loop* loop);
	~Redis();

	bool select(int dbi);

	void open(const char* hostname, int port);
	void open(const IPAddress& ipaddr, int port);
	bool isOpen() const;
	void close();

	void incr(const std::string& key);
	void decr(const std::string& key);

	// CacheService overrides
	virtual bool set(const char* key, const char* value);
	virtual bool set(const char* key, size_t keysize, const char* val, size_t valsize);
	virtual bool set(const BufferRef& key, const BufferRef& value);

//	virtual bool get(const char* key, Buffer& value);
	virtual bool get(const char* key, size_t keysize, Buffer& val);
//	virtual bool get(const BufferRef& key, Buffer& value);
	using CacheService::get;

	class Response;

protected:
	template<typename... Args>
	void writeMessage(const Args&... args);

	ssize_t flush();

	void readMessage();

private:
	typedef std::pair<const char*, size_t> MemRef;

	template<typename Arg1, typename... Args>
	void _writeArg(const Arg1& arg1, const Args&... args);

	void _writeArg(int arg);
	void _writeArg(const char* arg);
	void _writeArg(const std::string& arg);
	void _writeArg(const Buffer& arg);
	void _writeArg(const BufferRef& arg);
	void _writeArg(const MemRef& arg);

private:
	struct ev_loop* loop_;
	Socket* socket_;
	Buffer buf_;
	size_t flushPos_;
};

} // namespace x0

#endif
