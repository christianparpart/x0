#include <x0/http/HttpAuthUserFile.h>
#include <fstream>
#include <cstdio>
#include <fcntl.h>

namespace x0 {

HttpAuthUserFile::HttpAuthUserFile(const std::string& filename) :
	filename_(filename),
	users_()
{
}

HttpAuthUserFile::~HttpAuthUserFile()
{
}

bool HttpAuthUserFile::readFile()
{
	char buf[4096];
	std::ifstream in(filename_);
	users_.clear();

	while (in.good()) {
		in.getline(buf, sizeof(buf));
		size_t len = in.gcount();
		if (!len) continue;				// skip empty lines
		if (buf[0] == '#') continue;	// skip comments
		char *p = strchr(buf, ':');
		if (!p) continue;				// invalid line
		std::string name(buf, p - buf);
		std::string pass(1 + p, len - (p - buf) - 2);
		users_[name] = pass;
	}

	return !users_.empty();
}

bool HttpAuthUserFile::authenticate(const std::string& username, const std::string& passwd)
{
	if (!readFile())
		return false;

	auto i = users_.find(username);
	if (i != users_.end())
		return i->second == passwd;

	return false;
}

} // namespace x0
