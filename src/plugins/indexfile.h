/* <x0/plugins/indexfile.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009-2010 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_plugins_indexfile_h
#define sw_x0_plugins_indexfile_h (1)

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>

/**
 * \ingroup plugins
 * \brief implements automatic index file resolving, if mapped request path is a path.
 */
class indexfile_plugin :
	public x0::HttpPlugin
{
public:
	indexfile_plugin(x0::HttpServer& srv, const std::string& name);
	~indexfile_plugin();

	bool defaultEnabled() const;
	void setDefaultEnabled(bool value);

	std::vector<std::string> *indexFiles(const x0::Scope& scope) const;
	void setIndexFiles(x0::Scope& scope, const std::vector<std::string>& indexFiles);

private:
	x0::HttpServer::RequestHook::Connection c;

	struct context : public x0::ScopeValue
	{
		std::vector<std::string> index_files;

		virtual void merge(const x0::ScopeValue *value);
	};

	std::error_code setup_indexfiles(const x0::SettingsValue& cvar, x0::Scope& s);
	void indexfile(x0::HttpRequest *in);
};

#endif
