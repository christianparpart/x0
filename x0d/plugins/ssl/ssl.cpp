/* <x0/plugins/ssl/ssl.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include "SslContext.h"
#include "SslDriver.h"
#include "SslSocket.h"
#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/io/BufferSource.h>
#include <x0/SocketSpec.h>
#include <x0/strutils.h>
#include <x0/Types.h>

#include <cstring>
#include <cerrno>
#include <cstddef>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <pthread.h>
#include <gcrypt.h>
GCRY_THREAD_OPTION_PTHREAD_IMPL;

#if 0 // !defined(XZERO_NDEBUG)
#	define TRACE(msg...) DEBUG("ssl: " msg)
#else
#	define TRACE(msg...) /*!*/
#endif


/*
 * possible flow API:
 *
 *     void ssl.listen('IP:PORT');
 *     void ssl.listen('IP:PORT', backlog);
 *     void ssl.listen('IP:PORT', backlog, defaultKey, defaultCrt);
 *
 *     void ssl.add(hostname, certfile, keyfile);
 *
 *
 * EXAMPLE:
 *     ssl.listen '0.0.0.0:8443';
 *
 *     ssl.add 'hostname' => 'www.trapni.de',
 *             'certfile' => '/path/to/my.crt',
 *             'keyfile' => '/path/to/my.key',
 *             'crlfile' => '/path/to/my.crl';
 *
 */

/**
 * \ingroup plugins
 * \brief SSL plugin
 */
class SslPlugin :
	public x0d::XzeroPlugin,
	public SslContextSelector
{
private:
	std::list<x0::ServerSocket*> listeners_;
	std::string priorities_;

public:
	SslPlugin(x0d::XzeroDaemon* d, const std::string& name) :
		x0d::XzeroPlugin(d, name),
		listeners_(),
		priorities_("NORMAL")
	{
		gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);

		int rv = gnutls_global_init();
		if (rv != GNUTLS_E_SUCCESS) {
			TRACE("gnutls_global_init: %s", gnutls_strerror(rv));
			return; //Error::CouldNotInitializeSslLibrary;
		}
		TRACE("gnutls_global_init: %s", gnutls_strerror(rv));

		daemon().addComponent(std::string("GnuTLS/") + gnutls_check_version(nullptr));

        setupFunction("ssl.listen", &SslPlugin::add_listener)
            .param<x0::IPAddress>("address", x0::IPAddress("0.0.0.0"))
            .param<int>("port", 443)
            .param<int>("backlog", 128)
            .param<int>("multi_accept", 1)
            .param<bool>("reuse_port", false);

        setupFunction("ssl.loglevel", &SslPlugin::set_loglevel, x0::FlowType::Number);
        setupFunction("ssl.priorities", &SslPlugin::set_priorities, x0::FlowType::String);

        setupFunction("ssl.context", &SslPlugin::add_context)
            .param<x0::FlowString>("keyfile")
            .param<x0::FlowString>("certfile")
            .param<x0::FlowString>("trustfile", "")
            .param<x0::FlowString>("priorities", "");
	}

	~SslPlugin()
	{
		for (auto i: contexts_)
			delete i;

		gnutls_global_deinit();
	}

	std::vector<SslContext *> contexts_;

	/** select the SSL context based on host name or nullptr if nothing found. */
	virtual SslContext *select(const std::string& dnsName) const
	{
		if (dnsName.empty())
			return contexts_.front();

		for (auto cx: contexts_) {
			if (cx->isValidDnsName(dnsName)) {
				TRACE("select SslContext: CN:%s, dnsName:%s", cx->commonName().c_str(), dnsName.c_str());
				return cx;
			}
		}

		return nullptr;
	}

	virtual bool post_config()
	{
		for (auto listener: listeners_)
			static_cast<SslDriver*>(listener->socketDriver())->setPriorities(priorities_);

		for (auto cx: contexts_)
			cx->post_config();

		return true;
	}

	virtual bool post_check()
	{
		// TODO do some post-config checks here.
		return true;
	}

	// {{{ config
private:
	// ssl.listener(BINDADDR_PORT);
	void add_listener(x0::FlowVM::Params& args)
	{
        x0::SocketSpec socketSpec(
            args.getIPAddress(1),   // bind addr
            args.getInt(2),         // port
            args.getInt(3),         // backlog
            args.getInt(4),         // multi accept
            args.getBool(5)         // reuse port
        );

        x0::ServerSocket* listener = server().setupListener(socketSpec);
        if (listener) {
            SslDriver *driver = new SslDriver(this);
            listener->setSocketDriver(driver);
            listeners_.push_back(listener);
        }
	}

	void set_loglevel(x0::FlowVM::Params& args)
	{
        setLogLevel(args.getInt(1));
	}

	void set_priorities(x0::FlowVM::Params& args)
	{
        //TODO priorities_ = args[0].toString();
	}

	void setLogLevel(int value)
	{
		value = std::max(-10, std::min(10, value));
		TRACE("setLogLevel: %d", value);

		gnutls_global_set_log_level(value);
		gnutls_global_set_log_function(&SslPlugin::gnutls_logger);
	}

	static void gnutls_logger(int level, const char *message)
	{
		std::string msg(message);
		msg.resize(msg.size() - 1);

		TRACE("gnutls [%d] %s", level, msg.c_str());
	}

	// ssl.context(
    //         keyfile: PATH,
	// 	       certfile: PATH,
	// 	       trustfile: PATH,
	// 	       priorities: CIPHERS
    // );
	//
	void add_context(x0::FlowVM::Params& args)
	{
		std::auto_ptr<SslContext> cx(new SslContext());

		cx->setLogger(server().logger());

        cx->keyFile = args.getString(1).str();
        cx->certFile = args.getString(2).str();
        cx->trustFile = args.getString(3).str();
        std::string priorities = args.getString(4).str();

		// context setup successful -> put into our ssl context set.
		contexts_.push_back(cx.release());
	}
	// }}}
};

X0_EXPORT_PLUGIN_CLASS(SslPlugin)
