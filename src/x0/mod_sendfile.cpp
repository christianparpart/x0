/* <x0/mod_sendfile.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/types.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

/**
 * \ingroup modules
 * \brief serves static files from server's local filesystem to client.
 */
class sendfile_plugin :
	public x0::plugin
{
public:
	sendfile_plugin(x0::server& srv) :
		x0::plugin(srv)
	{
		server_.content_generator.connect(x0::bindMember(&sendfile_plugin::handler, this));
	}

	~sendfile_plugin() {
		server_.content_generator.disconnect(x0::bindMember(&sendfile_plugin::handler, this));
	}

private:
	bool handler(x0::request& in, x0::response& out) {
		std::string path(in.filename);

		struct stat st;
		if (stat(path.c_str(), &st) != 0)
			return false;

		int fd = open(path.c_str(), O_RDONLY);
		if (fd == -1)
		{
			// TODO log errno
			return false;
		}

		// XXX setup some response headers

		out.content.resize(st.st_size);
		::read(fd, &out.content[0], st.st_size);

		// XXX send out headers, as they're fixed size in user space.
		// XXX start async transfer through sendfile()

		return true;
	}
};

extern "C" void sendfile_init(x0::server& srv) {
	srv.setup_plugin(x0::plugin_ptr(new sendfile_plugin(srv)));
}
