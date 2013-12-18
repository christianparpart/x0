#include <x0/flow/FlowLocation.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace x0 {

std::string FlowLocation::dump(const std::string& prefix) const
{
	char buf[4096];
	std::size_t n = snprintf(buf, sizeof(buf), "%s: { %zu:%zu.%zu - %zu:%zu.%zu }",
		!prefix.empty() ? prefix.c_str() : "location",
		begin.line, begin.column, begin.offset,
		end.line, end.column, end.offset);
	return std::string(buf, n);
}

std::string FlowLocation::text() const
{
	std::string result;
	char* buf = nullptr;
	ssize_t size;
	ssize_t n;
	int fd;

	fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0)
		return std::string();

	size = 1 + end.offset - begin.offset;
	if (size <= 0)
		goto out;

	if (lseek(fd, begin.offset, SEEK_SET) < 0)
		goto out;

	buf = new char[size + 1];
	n = read(fd, buf, size); 
	if (n < 0)
		goto out;

	result = std::string(buf, n);

out:
	delete[] buf;
	close(fd);
	return result;
}

} // namespace
