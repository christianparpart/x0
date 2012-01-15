/* <x0/Redis.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/cache/Redis.h>
#include <cctype>

//#define TRACE(msg...) DEBUG("Redis: " msg)
#define TRACE(msg...) /*!*/ ((void)0)

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

// {{{ Message
Redis::Message::Message() :
	type_(Nil),
	number_()
{
}

Redis::Message::Message(Type t, char* buf, size_t size) :
	type_(t),
	number_(size),
	string_(buf)
{
}

Redis::Message::Message(Type t, long long value) :
	type_(t),
	number_(value)
{
}

Redis::Message::~Message()
{
	switch (type_) {
		case Status:
		case Error:
		case String:
			delete[] string_;
			break;
		case Array:
			delete[] array_;
			break;
		default:
			break;
	}
}

Redis::Message* Redis::Message::createNil()
{
	return new Message();
}

Redis::Message* Redis::Message::createStatus(const BufferRef& message)
{
	size_t size = message.size();
	char* buf = new char[size + 1];
	memcpy(buf, message.data(), size);
	buf[size] = 0;

	return new Message(Status, buf, size);
}

Redis::Message* Redis::Message::createError(const BufferRef& message)
{
	size_t size = message.size();
	char* buf = new char[size + 1];
	memcpy(buf, message.data(), size);
	buf[size] = 0;

	return new Message(Error, buf, size);
}

Redis::Message* Redis::Message::createNumber(long long value)
{
	return new Message(Number, value);
}

Redis::Message* Redis::Message::createString(const BufferRef& value)
{
	size_t size = value.size();
	char* buf = new char[size + 1];
	memcpy(buf, value.data(), size);
	buf[size] = 0;

	return new Message(String, buf, size);
}

Redis::Message* Redis::Message::createArray(size_t size)
{
	return new Message(Array, size);
}
// }}}

// {{{ MessageParser
Redis::MessageParser::MessageParser(const Buffer* buf) :
	buffer_(buf),
	pos_(0),
	currentContext_(nullptr),
	begin_(0),
	argSize_(0)
{
	pushContext(); // create root context
}

Redis::MessageParser::~MessageParser()
{
}

void Redis::MessageParser::parse()
{
	while (!isEndOfBuffer()) {
		if (std::isprint(currentChar()))
			TRACE("parse: '%c' (%d)", currentChar(), static_cast<int>(state()));
		else
			TRACE("parse: 0x%02X (%d)", currentChar(), static_cast<int>(state()));

		switch (state()) {
			case MESSAGE_BEGIN:
				// Syntetic state. Go straight to TYPE.
			case MESSAGE_TYPE:
				switch (currentChar()) {
					case '+':
					case '-':
					case ':':
						currentContext_->type = static_cast<Message::Type>(currentChar());
						setState(MESSAGE_LINE_BEGIN);
						nextChar();
						break;
					case '$':
						currentContext_->type = Message::String;
						setState(BULK_BEGIN);
						break;
					case '*':
						currentContext_->type = Message::Array;
						setState(MESSAGE_NUM_ARGS);
						nextChar();
						break;
					default:
						currentContext_->type = Message::Nil;
						setState(SYNTAX_ERROR);
						return;
				}
				break;
			case MESSAGE_LINE_BEGIN:
				if (currentChar() == '\r') {
					setState(SYNTAX_ERROR);
					return;
				}
				setState(MESSAGE_LINE_OR_CR);
				begin_ = pos_;
				nextChar();
				break;
			case MESSAGE_LINE_OR_CR:
				if (currentChar() == '\n') {
					setState(SYNTAX_ERROR);
					return;
				}

				if (currentChar() == '\r')
					setState(MESSAGE_LINE_LF);

				nextChar();
				break;
			case MESSAGE_LINE_LF: {
				if (currentChar() != '\n') {
					setState(SYNTAX_ERROR);
					return;
				}
				BufferRef value = buffer_->ref(begin_, pos_ - begin_ - 1);
				switch (currentContext_->type) {
					case Message::Status:
						currentContext_->message = Message::createStatus(value);
						break;
					case Message::Error:
						currentContext_->message = Message::createError(value);
						break;
					case Message::String:
						currentContext_->message = Message::createString(value);
						break;
					case Message::Number:
						currentContext_->message = Message::createNumber(value.toInt());
						break;
					default:
						currentContext_->message = Message::createNil();
						break;
				}
				setState(MESSAGE_END);
				nextChar();
				popContext();
				break;
			}
			case MESSAGE_NUM_ARGS: {
				switch (currentChar()) {
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
						currentContext_->number *= 10;
						currentContext_->number += currentChar() - '0';
						setState(MESSAGE_NUM_ARGS_OR_CR);
						nextChar();
						break;
					default:
						setState(SYNTAX_ERROR);
						return;
				}
				break;
			}
			case MESSAGE_NUM_ARGS_OR_CR:
				switch (currentChar()) {
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
						currentContext_->number *= 10;
						currentContext_->number += currentChar() - '0';
						nextChar();
						break;
					case '\r':
						setState(MESSAGE_LF);
						nextChar();
						break;
					default:
						setState(SYNTAX_ERROR);
						return;
				}
				break;
			case MESSAGE_LF:
				if (currentChar() != '\n') {
					setState(SYNTAX_ERROR);
					return;
				}

				nextChar();

				if (currentContext_->type == Message::Array) {
					setState(BULK_BEGIN);
					pushContext();
				} else {
					setState(MESSAGE_END);
					popContext();
				}
				break;
			case BULK_BEGIN:
				if (currentChar() != '$') {
					setState(SYNTAX_ERROR);
					return;
				}
				setState(BULK_SIZE);
				nextChar();
				break;
			case BULK_SIZE:
				switch (currentChar()) {
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
						argSize_ += currentChar() - '0';
						setState(BULK_SIZE_OR_CR);
						nextChar();
						break;
					default:
						setState(SYNTAX_ERROR);
						return;
				}
				break;
			case BULK_SIZE_OR_CR:
				switch (currentChar()) {
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
						argSize_ += currentChar() - '0';
						nextChar();
						break;
					case '\r':
						setState(BULK_SIZE_LF);
						nextChar();
						break;
					default:
						setState(SYNTAX_ERROR);
						return;
				}
				break;
			case BULK_SIZE_LF:
				if (currentChar() != '\n') {
					setState(SYNTAX_ERROR);
					return;
				}
				nextChar();
				setState(BULK_BODY_OR_CR);
				begin_ = pos_;
				break;
			case BULK_BODY_OR_CR:
				if (argSize_ > 0) {
					argSize_ -= nextChar(argSize_);
				} else if (currentChar() == '\r') {
					BufferRef value = buffer_->ref(begin_, pos_ - begin_);
					currentContext_->message = Message::createString(value);
					nextChar();
					setState(BULK_BODY_LF);
				} else {
					setState(SYNTAX_ERROR);
					return;
				}
				break;
			case BULK_BODY_LF:
				if (currentChar() != '\n') {
					setState(SYNTAX_ERROR);
					return;
				}
				nextChar();

				setState(MESSAGE_END);
				popContext();

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

inline void Redis::MessageParser::nextChar()
{
	if (!isEndOfBuffer()) {
		++pos_;
	}
}

inline size_t Redis::MessageParser::nextChar(size_t n)
{
	size_t avail = buffer_->size() - pos_;
	n = std::min(n, avail);
	pos_ += n;
	return n;
}

inline BufferRef Redis::MessageParser::currentValue() const
{
	return buffer_->ref(begin_, pos_ - begin_);
}

void Redis::MessageParser::pushContext()
{
	TRACE("pushContext:");
	ParseContext* pc = new ParseContext();
	pc->parent = currentContext_;
	currentContext_ = pc;

	setState(MESSAGE_BEGIN);
}

void Redis::MessageParser::popContext()
{
	TRACE("popContext:");
	ParseContext* pc = currentContext_;

	if (!pc->parent) {
		TRACE("popContext: do not pop. we're already at root.");
		return;
	}

	currentContext_ = currentContext_
		? currentContext_->parent
		: nullptr;

	pc->parent = nullptr;

	delete pc;
}
// }}}

// {{{ Redis
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
	MessageParser parser(&buf_);

	socket_->read(buf_);
	parser.parse();

	if (parser.state() != MessageParser::MESSAGE_END) {
		// protocol error
		TRACE("protocol error: %d", parser.state());
		return false;
	}

	Message* message = parser.message();

	switch (message->type()) {
		case Message::Array:
		case Message::Status:
		case Message::Number:
			// unexpected result type, but yeah
		case Message::String:
			val.clear();
			val.push_back(message->toString());
			return true;
		case Message::Nil:
			val.clear();
			return true;
		case Message::Error:
		default:
			TRACE("unknown type");
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

// }}}

} // namespace x0
