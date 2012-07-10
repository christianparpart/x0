#pragma once

#include <x0/Api.h>
#include <x0/http/HttpAuthBackend.h>
#include <unordered_map>

namespace x0 {

class X0_API HttpAuthUserFile :
	public HttpAuthBackend
{
private:
	std::string filename_;
	std::unordered_map<std::string, std::string> users_;

public:
	explicit HttpAuthUserFile(const std::string& filename);
	~HttpAuthUserFile();

	virtual bool authenticate(const std::string& username, const std::string& passwd);

private:
	bool readFile();
};

} // namespace x0
