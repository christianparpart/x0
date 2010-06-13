#include <x0/http/HttpPlugin.h>

namespace x0 {

/** \brief initializes the plugin.
  *
  * \param srv reference to owning server object
  * \param name unique and descriptive plugin-name.
  */
HttpPlugin::HttpPlugin(HttpServer& srv, const std::string& name) :
	server_(srv),
	name_(name)
#if !defined(NDEBUG)
	, debug_level_(9)
#endif
{
}

/** \brief safely destructs the plugin.
  */
HttpPlugin::~HttpPlugin()
{
	while (!cvars_.empty())
		undeclareCVar(cvars_[0]);
}

/** \brief retrieves the number of configuration variables registered by this plugin.
  */
const std::vector<std::string>& HttpPlugin::cvars() const
{
	return cvars_;
}

/** \brief unregisters given configuration variable.
  */
void HttpPlugin::undeclareCVar(const std::string& key)
{
	for (auto i = cvars_.begin(), e = cvars_.end(); i != e; ++i)
	{
		if (*i == key)
		{
			server_.undeclareCVar(key);
			cvars_.erase(i);
			return;
		}
	}
}

void HttpPlugin::post_config()
{
}

} // namespace x0
