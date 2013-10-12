#include <x0/http/HttpFileMgr.h>
#include <x0/Buffer.h>
#include <x0/Tokenizer.h>

namespace x0 {

HttpFileMgr::HttpFileMgr(ev::loop_ref loop, Settings* settings) :
	loop_(loop),
	settings_(settings),
	files_()
{
}

HttpFileMgr::~HttpFileMgr()
{
}

HttpFileRef HttpFileMgr::query(const std::string& path)
{
	auto i = files_.find(path);
	if (i != files_.end())
		return i->second;

	return files_[path] = HttpFileRef(new HttpFile(path, this));
}

bool readFile(const std::string& filename, Buffer* output)
{
	int fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0)
		return false;

	struct stat st;
	if (fstat(fd, &st) < 0)
		return false;

	output->reserve(st.st_size + 1);
	ssize_t nread = ::read(fd, output->data(), st.st_size);
	if (nread < 0) {
		::close(fd);
		return false;
	}

	output->data()[nread] = '\0';
	output->resize(nread);
	::close(fd);

	return true;
}

bool HttpFileMgr::Settings::openMimeTypes(const std::string& filename)
{
	Buffer input;
	if (!readFile(filename, &input))
		return false;

	auto lines = Tokenizer<BufferRef, Buffer>::tokenize(input, "\n");

	mimetypes.clear();

	for (auto line: lines) {
		line = line.trim();
		auto columns = Tokenizer<BufferRef, BufferRef>::tokenize(line, " \t");

		auto ci = columns.begin(), ce = columns.end();
		BufferRef mime = ci != ce ? *ci++ : BufferRef();

		if (!mime.empty() && mime[0] != '#') {
			for (; ci != ce; ++ci) {
				mimetypes[ci->str()] = mime.str();
			}
		}
	}

	return true;
}

} // namespace x0
