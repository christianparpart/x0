
// {{{ rack-like API (crack)
namespace CRack {

class Request
{
	// input
	virtual BufferRef method() const = 0;
	virtual BufferRef uri() const = 0;
	virtual BufferRef path() const = 0;
	virtual BufferRef query() const = 0;
	virtual BufferRef scriptName() const = 0;
	virtual BufferRef pathInfo() const = 0;
	virtual BufferRef getHeader(const char *name) const = 0;

	virtual int contentLength() const = 0;
	virtual int read(char *buf, size_t size) = 0;

	// output
	virtual void setResponseHeader(const char *key, const char *value);
	virtual int write(const char *buf, size_t len) = 0;
	virtual int flush() = 0;

	virtual void log(const char *fmt, ...) = 0;

	virtual void finish();
};
} // namespace CRack

// }}}

// {{{ fastcgi
namespace CRack {
namespace FCgi {

class Service;
class Transport;
class Request;

class Transport
{
private:
	Service *service_;
	int fd_;

	Buffer readBuffer_;
	size_t readOffset_;
	size_t readSize_;//== readBuffer_.size() ?

	Buffer writeBuffer_;
	size_t writeOffset_;

	HashMap<int, Request *> requests_;

public:
	void write(FastCgi::Record *record);
	void flush();

private:
	void io(ev::io& w, int revents);

	void process(FastCgi::Record *r);
	void handleRequest(Request *r);

	// server-to-application
	void beginRequest(FastCgi::BeginRequestRecord *r);
	void streamParams(FastCgi::Record *r);
	void streamStdin(FastCgi::Record *r);
	void streamData(FastCgi::Record *r);
	void abortRequest(FastCgi::Record *r);

	// application->to-server
	void processStdout(const char *buf, size_t size);
	void processStderr(const char *buf, size_t size);
	void process(FastCgi::EndRequestRecord *r);
};

class Request :
	public CRack::Request,
	public FastCgi::CgiParamStreamReader
{
private:
	friend class Service;
	int fd_;                    // transport socket descriptor
	ev::io io_;
	ev::timer timer_;

public:
};

class Service
{
public:
	Service();
	~Service();

	int listen(const char *bind, int port, int backlog = 128);
	void stop(int listener);

	virtual void handleRequest(CgiRequest *r);
};

} // namespace FCgi
} // namespace CRack
// }}}
