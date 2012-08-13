/* <Redis.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

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

	// connection
	void open(const char* hostname, int port);
	void open(const IPAddress& ipaddr, int port);
	bool isOpen() const;
	void close();

	// database
	bool select(int dbi);

	// value: numerical operations
	void incr(const std::string& key);
	void decr(const std::string& key);

	// keys
	void expire(const char* key, time_t timeout);
	time_t ttl(const char* key);
	void persist(const char* key);

	bool keys(const char* pattern, char** result);

	// server: replication
	void setSlaveOf(const std::string& remote);
	void clearSlaveOf();

	// CacheService overrides
	virtual bool set(const char* key, const char* value);
	virtual bool set(const char* key, size_t keysize, const char* val, size_t valsize);
	virtual bool set(const BufferRef& key, const BufferRef& value);

//	virtual bool get(const char* key, Buffer& value);
	virtual bool get(const char* key, size_t keysize, Buffer& val);
//	virtual bool get(const BufferRef& key, Buffer& value);
	using CacheService::get;

	class Message;
	class MessageParser;

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

class Redis::Message // {{{
{
public:
	enum Type {
		Nil = 0,
		Status = '+',
		Error = '-',
		Number = ':',
		String = '$',
		Array = '*',
	};

private:
	Message();
	Message(Type t, char* buf, size_t size);
	Message(Type t, long long value);

public:
	~Message();

	static Message* createNil();
	static Message* createStatus(const BufferRef& value);
	static Message* createError(const BufferRef& value);
	static Message* createNumber(long long value);
	static Message* createString(const BufferRef& value);
	static Message* createArray(size_t size);

	// type checker
	inline Type type() const { return type_; }
	inline bool isNil() const { return type_ == Nil; }
	inline bool isStatus() const { return type_ == Status; }
	inline bool isError() const { return type_ == Error; }
	inline bool isNumber() const { return type_ == Number; }
	inline bool isString() const { return type_ == String; }
	inline bool isArray() const { return type_ == Array; }

	// value setter
	void setNil();
	void setNumber(long long value);
	void setString(const char* value, size_t size);
	void setArray(size_t size);

	// value getter
	long long toNumber() const { return number_; }
	const char* toString() const { return string_; }
	const Message* toArray() const { return array_; }
	const Message& operator[](size_t i) const { return array_[i]; }
	inline size_t size() const { return number_; }

private:
	Type type_;
	long long number_;
	union {
		char* string_;
		Message* array_;
	};
};
// }}}

class Redis::MessageParser // {{{
{
public:
	enum State {
		MESSAGE_BEGIN,

		MESSAGE_TYPE,			// $ : + - *
		MESSAGE_LINE_BEGIN,		// ...
		MESSAGE_LINE_OR_CR,		// ... \r
		MESSAGE_LINE_LF,		//     \n
		MESSAGE_NUM_ARGS,		// 123
		MESSAGE_NUM_ARGS_OR_CR,	// 123 \r
		MESSAGE_LF,				//     \n

		BULK_BEGIN,				// $
		BULK_SIZE,				// 1234
		BULK_SIZE_OR_CR,		// 1234 \r
		BULK_SIZE_LF,			//      \n
		BULK_BODY_OR_CR,		// ...  \r
		BULK_BODY_LF,			//      \n

		MESSAGE_END,
		SYNTAX_ERROR
	};

public:
	explicit MessageParser(const Buffer* buf);
	~MessageParser();

	void parse();

	Message* message() const { return currentContext_->message; }

	inline bool isSyntaxError() const { return state() == SYNTAX_ERROR; }

	inline bool isEndOfBuffer() const { return pos_ == buffer_->size(); }
	inline char currentChar() const { return (*buffer_)[pos_]; }
	inline void nextChar();
	inline size_t nextChar(size_t n);
	inline BufferRef currentValue() const;

	State state() const { return currentContext_->state; }
	inline void setState(State st) { currentContext_->state = st; }

private:
	struct ParseContext
	{
		ParseContext() :
			parent(nullptr),
			type(Message::Nil),
			state(MESSAGE_BEGIN),
			number(0),
			sign(false),
			message(nullptr)
		{
		}

		ParseContext* parent;
		Message::Type type;
		State state;			//!< current parser-state
		long long number;		//!< a parsed "number" (array length, string/error/status length, integer value)
		bool sign;				//!< number-sign flag (true if sign-symbol was parsed)
		Message* message;		//!< message
	};

	// global parser state
	const Buffer* buffer_;		//!< buffer holding the message
	size_t pos_;				//!< current parse byte-pos

	ParseContext* currentContext_;

	// bulk context
	size_t begin_;				//!< first byte of currently parsed argument
	ssize_t argSize_;			//!< size of the currently parsed argument

	void pushContext();
	ParseContext* currentContext() const { return currentContext_; }
	void popContext();
}; // }}}

} // namespace x0

#endif
