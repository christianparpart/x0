/* <x0/StringError.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/StringError.h>

#include <vector>

namespace x0 {

StringErrorCategoryImpl::StringErrorCategoryImpl()
{
	vector_.push_back("Success");
	vector_.push_back("Generic Error");
}

StringErrorCategoryImpl::~StringErrorCategoryImpl()
{
}

int StringErrorCategoryImpl::get(const std::string& msg)
{
	if (msg.empty())
		return 0;

	for (int i = 0, e = vector_.size(); i != e; ++i)
		if (vector_[i] == msg)
			return i;

	vector_.push_back(msg);
	return vector_.size() - 1;
}

const char *StringErrorCategoryImpl::name() const
{
	return "custom";
}

std::string StringErrorCategoryImpl::message(int ec) const
{
	return vector_[ec];
}

StringErrorCategoryImpl& stringErrorCategory() throw()
{
	static StringErrorCategoryImpl impl;
	return impl;
}

} // namespace x0
