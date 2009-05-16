#include <http/mime_types.hpp>

namespace http {
namespace mime_types {

struct mapping
{
	const char *extension;
	const char *mime_type;
} mappings[] = {
	{ "html", "text/html" },
	{ "css", "text/css" },
	{ "gif", "image/gif" },
	{ "png", "image/png" },
	{ "jpg", "image/jpeg" },
	{ 0, 0 },
};

std::string extension_to_type(const std::string& extension)
{
	for (mapping *m = mappings; m->extension; ++m)
	{
		if (m->extension == extension)
		{
			return m->mime_type;
		}
	}
	return "text/plain";
}

} // namespace mime_types
} // namespace http
