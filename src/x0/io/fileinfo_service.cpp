#include <x0/io/fileinfo_service.hpp>
#include <x0/strutils.hpp>

#include <boost/tokenizer.hpp>

namespace x0 {

void fileinfo_service::load_mimetypes(const std::string& filename)
{
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

	std::string input(x0::read_file(filename));
	tokenizer lines(input, boost::char_separator<char>("\n"));

	mimetypes_.clear();

	for (tokenizer::iterator i = lines.begin(), e = lines.end(); i != e; ++i)
	{
		std::string line(x0::trim(*i));
		tokenizer columns(line, boost::char_separator<char>(" \t"));

		tokenizer::iterator ci = columns.begin(), ce = columns.end();
		std::string mime = ci != ce ? *ci++ : std::string();

		if (!mime.empty() && mime[0] != '#')
		{
			for (; ci != ce; ++ci)
			{
				mimetypes_[*ci] = mime;
			}
		}
	}
}

} // namespace x0
