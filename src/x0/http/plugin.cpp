#include <x0/http/plugin.hpp>

namespace x0 {

/** \brief initializes the plugin.
  *
  * \param srv reference to owning server object
  * \param name unique and descriptive plugin-name.
  */
plugin::plugin(server& srv, const std::string& name) :
	server_(srv),
	name_(name)
#if !defined(NDEBUG)
	, debug_level_(9)
#endif
{
}

/** \brief safely destructs the plugin.
  */
plugin::~plugin()
{
	while (!cvars_.empty())
		unregister_cvar(cvars_[0]);
}

/** \brief retrieves the number of configuration variables registered by this plugin.
  */
const std::vector<std::string>& plugin::cvars() const
{
	return cvars_;
}

/** \brief unregisters given configuration variable.
  */
void plugin::unregister_cvar(const std::string& key)
{
	for (auto i = cvars_.begin(), e = cvars_.end(); i != e; ++i)
	{
		if (*i == key)
		{
			cvars_.erase(i);
			server_.unregister_cvar(key);
			return;
		}
	}
}

void plugin::post_config()
{
}

} // namespace x0
