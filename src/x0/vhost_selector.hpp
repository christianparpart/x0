#ifndef x0_vhost_selector_hpp
#define x0_vhost_selector_hpp (1)

namespace x0 {

struct vhost_selector
{
	std::string hostname;
	int port;

	vhost_selector(const std::string _host, int _port) :
		hostname(_host), port(_port)
	{
	}

	friend int compare(const vhost_selector& a, const vhost_selector& b)
	{
		if (&a == &b)
			return 0;

		if (a.hostname < b.hostname)
			return -1;

		if (a.hostname > b.hostname)
			return 1;

		if (a.port < b.port)
			return -1;

		if (a.port > b.port)
			return 1;

		return 0;
	}

	friend bool operator<(const vhost_selector& a, const vhost_selector& b)
	{
		return compare(a, b) < 0;
	}

	friend bool operator==(const vhost_selector& a, const vhost_selector& b)
	{
		return compare(a, b) == 0;
	}
};

} // namespace x0

#endif
