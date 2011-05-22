/* <flow/FlowValue.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.ws/projects/flow
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/flow/FlowValue.h>
#include <x0/IPAddress.h>
#include <x0/RegExp.h>
#include <cstring>

namespace x0 {

void FlowValue::dump() const
{
	dump(true);
}

void FlowValue::dump(bool x) const
{
	fflush(stderr);
	switch (type_)
	{
		case FlowValue::VOID:
			printf("void");
			break;
		case FlowValue::BOOLEAN:
			printf(toBool() ? "true" : "false");
			break;
		case FlowValue::NUMBER:
			printf("%lld", toNumber());
			break;
		case FlowValue::REGEXP:
			printf("/%s/", toRegExp().c_str());
			break;
		case FlowValue::IP:
			printf("ip(%s)", toIPAddress().str().c_str());
			break;
		case FlowValue::FUNCTION:
			printf("fnref(0x%p)", reinterpret_cast<void*>(toFunction()));
			break;
		case FlowValue::STRING:
			printf("'%s'", toString());
			break;
		case FlowValue::BUFFER: {
			long long length = toNumber();
			const char *p = toString();
			std::string data(p, p + length);
			printf("'%s'", data.c_str());
			break;
		}
		case FlowValue::ARRAY: {
			const FlowArray& p = toArray();
			printf("[");
			for (size_t k = 0, ke = p.size(); k != ke; ++k) {
				if (k) printf(", ");
				p[k].dump(false);
			}
			printf("]");

			break;
		}
		default:
			break;
	}

	if (x)
		printf("\n");
}

} // namespace x0
