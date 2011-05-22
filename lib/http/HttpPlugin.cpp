/* <x0/HttpPlugin.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpRequest.h>
#include <x0/SocketSpec.h>

namespace x0 {

// {{{ Params impl
SocketSpec& operator<<(SocketSpec& spec, const Params& params)
{
	// args usage (TCP): 'bind' => address, 'port' => num, ['backlog' => num]
	//           (unix): 'bind' => path, ['backlog' => num]

	spec.clear();

	unsigned mask = 0;
	enum Item {
		None    = 0x0000,
		Address = 0x0001,
		Port    = 0x0002,
		Backlog = 0x0004
	};

	for (auto& arg: params) {
		if (arg.isArray()) {
			const FlowArray& r = arg.toArray();
			if (r.size() != 2)
				goto err;

			if (!r[0].isString())
				goto err;

			std::string key(r[0].toString());

			Item m = Item::None;
			if (key == "bind") {
				m = Item::Address;
				if (r[1].isIPAddress()) {
					spec.address = r[1].toIPAddress();
				} else if (r[1].isString()) {
					spec.local = r[1].toString();
				} else {
					fprintf(stderr, "Invalid bind address specified (must be a path-string or IP address).\n");
					goto err;
				}
			} else if (key == "port") {
				m = Item::Port;
				if (r[1].isNumber())
					spec.port = r[1].toNumber();
				else {
					fprintf(stderr, "Invalid port number given (must be a number).\n");
					goto err;
				}
			} else if (key == "backlog") {
				m = Item::Backlog;
				if (r[1].isNumber())
					spec.backlog = r[1].toNumber();
				else {
					fprintf(stderr, "Invalid backlog size given (must be a number).\n");
					goto err;
				}
			} else {
				fprintf(stderr, "Unknown SocketSpec key: '%s'.\n", key.c_str());
				goto err;
			}

			if (mask & m) {
				fprintf(stderr, "multiple overriding SocketSpec found.\n");
				goto err;
			}

			mask |= m;
		}
	}

	if (!(mask & Item::Address)) {
		fprintf(stderr, "No host address given.");
		goto err;
	}

	if (spec.isLocal() && spec.port >= 0) {
	}

	spec.valid = true;
	return spec;

err:
	spec.valid = false;
	return spec;
}
// }}}

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
	// ensure that it's only the base-name we store
	// (fixes some certain cases where we've a path prefix supplied.)
	size_t i = name_.rfind('/');
	if (i != std::string::npos)
		name_ = name_.substr(i + 1);
}

/** \brief safely destructs the plugin.
  */
HttpPlugin::~HttpPlugin()
{
}

bool HttpPlugin::post_config()
{
	return true;
}

/** post-check hook, gets invoked after <b>every</b> configuration hook has been proceed successfully.
 *
 * \retval true everything turned out well.
 * \retval false something went wrong and x0 should not startup.
 */
bool HttpPlugin::post_check()
{
	return true;
}

} // namespace x0
