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

class IIndexFilePlugin
{
public:
	virtual ~IIndexFilePlugin() {}

	//virtual bool defaultEnabled() const = 0;
	//virtual void setDefaultEnabled(bool value) = 0;

	virtual std::vector<std::string> *indexFiles(const x0::Scope *scope) const = 0;
	virtual void setIndexFiles(x0::Scope& scope, const std::vector<std::string>& indexFiles) = 0;
};

/**
 * \ingroup plugins
 * \brief implements automatic index file resolving, if mapped request path is a path.
 */
class indexfile_plugin :
	public x0::HttpPlugin,
	public IIndexFilePlugin
{
public:
	indexfile_plugin(x0::HttpServer& srv, const std::string& name);
	~indexfile_plugin();

public: // IIndexFilePlugin
	//virtual bool defaultEnabled() const;
	//virtual void setDefaultEnabled(bool value);

	virtual std::vector<std::string> *indexFiles(const x0::Scope *scope) const;
	virtual void setIndexFiles(x0::Scope& scope, const std::vector<std::string>& indexFiles);

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
