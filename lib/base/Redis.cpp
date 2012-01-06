/* <x0/Redis.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/cache/Redis.h>
#include <cctype>

namespace x0 {

// {{{ request writer
template<typename... Args>
void Redis::writeMessage(const Args&... args)
{
	buf_.clear();
	buf_ << '*' << sizeof...(args) << "\r\n";
	_writeArg(args...);
}

template<typename Arg1, typename... Args>
void Redis::_writeArg(const Arg1& arg1, const Args&... args)
{
	_writeArg(arg1);
	_writeArg(args...);
}

void Redis::_writeArg(int arg)
{
	char buf[80];
	int n = snprintf(buf, sizeof(buf), "%d", arg);
	buf_ << '$' << n << "\r\n" << arg << "\r\n";
}

void Redis::_writeArg(const char* arg)
{
	int n = strlen(arg);
	buf_ << '$' << n << "\r\n" << arg << "\r\n";
}

void Redis::_writeArg(const std::string& arg)
{
	buf_ << '$' << arg.size() << "\r\n" << arg << "\r\n";
}

void Redis::_writeArg(const Buffer& arg)
{
	buf_ << '$' << arg.size() << "\r\n" << arg << "\r\n";
}

void Redis::_writeArg(const BufferRef& arg)
{
	buf_ << '$' << arg.size() << "\r\n" << arg << "\r\n";
}

void Redis::_writeArg(const MemRef& arg)
{
	buf_.push_back("$");
	buf_.push_back(arg.second);
	buf_.push_back("\r\n");
	buf_.push_back(arg.first, arg.second);
	buf_.push_back("\r\n");
}
// }}}

// {{{ response parser
class Redis::Response
{
public:
	enum Type {
		Unknown = 0,
		SingleLine = '+',
		Error = '-',
		Integer = ':',
		Bulk = '$',
		MultiBulk = '*'
	};

public:
	explicit Response(const Buffer* buf);
	~Response();

	Type type() const { return type_; }
	bool isPrimitive() const { return type_ == SingleLine; }
	bool isError() const { return type_ == Error; }
	bool isInteger() const { return type_ == Integer; }
	bool isBulk() const { return type_ == Bulk; }
	bool isMultiBulk() const { return type_ == MultiBulk; }

	size_t size() const { return arguments_.size(); }
	const BufferRef& operator[](size_t index) const { return arguments_[index]; }

protected:
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

	friend class Redis;

	void parse();

	inline bool isSyntaxError() const { return state_ == SYNTAX_ERROR; }
	inline bool isEndOfBuffer() const { return pos_ == buffer_->size(); }
	inline char current() const { return (*buffer_)[pos_]; }
	inline void next();
	inline size_t next(size_t n);
	inline void pushArgument();

private:
	const Buffer* buffer_;	//!< buffer holding the message
	size_t pos_;			//!< current parse byte-pos
	State state_;			//!< current parser-state
	size_t begin_;			//!< first byte of currently parsed argument
	ssize_t argSize_;		//!< size of the currently parsed argument
	ssize_t numArgs_;					//!< will hold the number of arguments in a multi-bulk message
	Type type_;							//!< message type
	std::vector<BufferRef> arguments_;	//!< parsed message arguments
};

Redis::Response::Response(const Buffer* buf) :
	buffer_(buf),
	pos_(0),
	state_(MESSAGE_BEGIN),
	begin_(0),
	argSize_(0),
	numArgs_(0),
	type_(Unknown),
	arguments_()
{
}

Redis::Response::~Response()
{
}

void Redis::Response::parse()
{
	while (!isEndOfBuffer()) {
		switch (state_) {
			case MESSAGE_BEGIN:
				// Syntetic state. Go straight to TYPE.
			case MESSAGE_TYPE:
				switch (current()) {
					case '+':
					case '-':
					case ':':
						type_ = static_cast<Type>(current());
						numArgs_ = 1;
						state_ = MESSAGE_LINE_BEGIN;
						next();
						break;
					case '$':
						type_ = static_cast<Type>(current());
						numArgs_ = 1;
						state_ = BULK_BEGIN;
						break;
					case '*':
						type_ = static_cast<Type>(current());
						state_ = MESSAGE_NUM_ARGS;
						next();
						break;
					default:
						type_ = Unknown;
						state_ = SYNTAX_ERROR;
						return;
				}
				break;
			case MESSAGE_LINE_BEGIN:
				if (current() == '\r') {
					state_ = SYNTAX_ERROR;
					return;
				}
				state_ = MESSAGE_LINE_OR_CR;
				begin_ = pos_;
				next();
				break;
			case MESSAGE_LINE_OR_CR:
				if (current() == '\n') {
					state_ = SYNTAX_ERROR;
					return;
				}

				if (current() == '\r')
					state_ = MESSAGE_LINE_LF;

				break;
			case MESSAGE_LINE_LF:
				if (current() != '\n') {
					state_ = SYNTAX_ERROR;
					return;
				}
				pushArgument();
				state_ = MESSAGE_END;
				next();
				break;
			case MESSAGE_NUM_ARGS:
				switch (current()) {
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						numArgs_ *= 10;
						numArgs_ += current() - '0';
						state_ = MESSAGE_NUM_ARGS_OR_CR;
						next();
						break;
					default:
						state_ = SYNTAX_ERROR;
						return;
				}
				break;
			case MESSAGE_NUM_ARGS_OR_CR:
				switch (current()) {
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						numArgs_ *= 10;
						numArgs_ += current() - '0';
						next();
						break;
					case '\r':
						state_ = MESSAGE_LF;
						next();
						break;
					default:
						state_ = SYNTAX_ERROR;
						return;
				}
				break;
			case MESSAGE_LF:
				if (current() != '\n') {
					state_ = SYNTAX_ERROR;
					return;
				}

				if (type_ == MultiBulk)
					state_ = BULK_BEGIN;
				else
					state_ = MESSAGE_END;

				next();
				break;
			case BULK_BEGIN:
				if (current() != '$') {
					state_ = SYNTAX_ERROR;
					return;
				}
				state_ = BULK_SIZE;
				next();
				break;
			case BULK_SIZE:
				switch (current()) {
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						argSize_ *= 10;
						argSize_ += current() - '0';
						state_ = BULK_SIZE_OR_CR;
						next();
						break;
					default:
						state_ = SYNTAX_ERROR;
						return;
				}
				break;
			case BULK_SIZE_OR_CR:
				switch (current()) {
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						argSize_ *= 10;
						argSize_ += current() - '0';
						next();
						break;
					case '\r':
						state_ = BULK_SIZE_LF;
						next();
						break;
					default:
						state_ = SYNTAX_ERROR;
						return;
				}
				break;
			case BULK_SIZE_LF:
				if (current() != '\n') {
					state_ = SYNTAX_ERROR;
					return;
				}
				next();
				state_ = BULK_BODY_OR_CR;
				begin_ = pos_;
				break;
			case BULK_BODY_OR_CR:
				if (argSize_ > 0) {
					argSize_ -= next(argSize_);
				} else if (current() == '\r') {
					pushArgument();
					next();
					state_ = BULK_BODY_LF;
				} else {
					state_ = SYNTAX_ERROR;
					return;
				}
				break;
			case BULK_BODY_LF:
				if (current() != '\n') {
					state_ = SYNTAX_ERROR;
					return;
				}

				--numArgs_;

				if (numArgs_ > 0)
					state_ = BULK_BEGIN;
				else
					state_ = MESSAGE_END;

				next();
				break;
			case MESSAGE_END:
				// if we reach here, then only because
				// there's garbage at the end of our message.
				break;
			case SYNTAX_ERROR:
				fprintf(stderr, "Redis message syntax error at offset %zi\n", pos_);
				break;
			default:
				break;
		}
	}
}

inline void Redis::Response::next()
{
	if (!isEndOfBuffer()) {
		++pos_;
	}
}

inline size_t Redis::Response::next(size_t n)
{
	size_t avail = buffer_->size() - pos_;
	n = std::min(n, avail);
	pos_ += n;
	return n;
}

inline void Redis::Response::pushArgument()
{
	auto ref = buffer_->ref(begin_, pos_ - begin_);
	arguments_.push_back(ref);
	//arguments_.push_back(buffer_->ref(begin_, pos_ - begin_));
}
// }}}

Redis::Redis(struct ev_loop* loop) :
	loop_(loop),
	socket_(new Socket(loop)),
	buf_(),
	flushPos_(0)
{
}

Redis::~Redis()
{
	close();
	delete socket_;
}

void Redis::open(const char* hostname, int port)
{
	socket_->openTcp(hostname, port);
}

void Redis::open(const IPAddress& ipaddr, int port)
{
	socket_->openTcp(ipaddr, port);
}

bool Redis::isOpen() const
{
	return socket_ && socket_->isOpen();
}

void Redis::close()
{
	if (!isOpen())
		return;

	socket_->close();
}

bool Redis::select(int dbi)
{
	writeMessage("SELECT", dbi);
	flush();

	buf_.clear();
	socket_->read(buf_);

	if (buf_ == "+OK\r\n")
		return true;

	return false;
}

bool Redis::set(const char* key, const char* value)
{
	writeMessage("SET", key, value);
	flush();

	buf_.clear();
	socket_->read(buf_);

	return true;
}

bool Redis::set(const BufferRef& key, const BufferRef& value)
{
	writeMessage("SET", key, value);
	flush();

	buf_.clear();
	socket_->read(buf_);

	return true;
}

void Redis::incr(const std::string& key)
{
	writeMessage("INCR", key);
	flush();

	buf_.clear();
	socket_->read(buf_);
}

void Redis::decr(const std::string& key)
{
	writeMessage("DECR", key);
	flush();

	buf_.clear();
	socket_->read(buf_);
}

bool Redis::set(const char* key, size_t keysize, const char* val, size_t valsize)
{
	writeMessage("SET", MemRef(key, keysize), MemRef(val, valsize));
	flush();

	buf_.clear();
	socket_->read(buf_);

	if (buf_ == "+OK\r\n")
		return true;

	return false;
}

bool Redis::get(const char* key, size_t keysize, Buffer& val)
{
	writeMessage("GET", MemRef(key, keysize));
	flush();

	buf_.clear();
	Response response(&buf_);

	socket_->read(buf_);
	response.parse();

	if (response.state_ != Response::MESSAGE_END) {
		// protocol error
		return false;
	}

	switch (response.type()) {
		case Response::MultiBulk:
		case Response::SingleLine:
		case Response::Integer:
			// unexpected result type, but yeah
		case Response::Bulk:
			val.clear();
			val.push_back(response[0]);
			return true;
		case Response::Error:
		case Response::Unknown:
		default:
			return false;
	}
}

ssize_t Redis::flush()
{
	ssize_t n = socket_->write(buf_.data() + flushPos_, buf_.size() - flushPos_);

	if (n > 0) {
		flushPos_ += n;
		if (flushPos_ == buf_.size()) {
			flushPos_ = 0;
			buf_.clear();
		}
	}
	return n;
}

} // namespace x0
