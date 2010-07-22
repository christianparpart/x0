/* <ICompressPlugin.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_plugins_ICompressPlugin_h
#define sw_x0_plugins_ICompressPlugin_h (1)

#include <string>
#include <vector>

class ICompressPlugin
{
public:
	virtual ~ICompressPlugin() {}

	virtual void setCompressTypes(const std::vector<std::string>& value) = 0;
	virtual void setCompressLevel(int value) = 0;
	virtual void setCompressMinSize(int value) = 0;
	virtual void setCompressMaxSize(int value) = 0;
};

#endif
