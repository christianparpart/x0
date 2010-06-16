#ifndef sw_x0_plugins_compress_h
#define sw_x0_plugins_compress_h

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
