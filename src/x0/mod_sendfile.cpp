#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <boost/lexical_cast.hpp>

#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace x0 {

bool sendfile_handler(request& in, response& out) {
	std::string path("." + in.uri);

	struct stat st;
	if (stat(path.c_str(), &st) != 0)
		return false;

	int fd = open(path.c_str(), O_RDONLY);
	if (fd == -1)
	{
		// TODO log errno
		return false;
	}
	std::cout << "sendfile:" << std::endl;

	// XXX setup some response headers
	out *= header("Content-Type", "text/plain");

	out.content.resize(st.st_size);
	::read(fd, &out.content[0], st.st_size);

	// XXX send out headers, as they're fixed size in user space.
	// XXX start async transfer through sendfile()

	return true;
}

void sendfile_init(server& srv) {
	srv.content_generator.connect(&sendfile_handler);
}

void sendfile_fini(server& srv) {
	srv.content_generator.disconnect(&sendfile_handler);
}

} // namespace x0
