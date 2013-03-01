/* <flow/FlowValue.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.io/projects/flow
 *
 * (c) 2010-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/FlowValue.h>
#include <x0/IPAddress.h>
#include <x0/SocketSpec.h>
#include <x0/RegExp.h>
#include <cstring>

namespace x0 {

std::string FlowValue::asString() const
{
	switch (type()) {
		case STRING:
			return toString();
		case REGEXP:
			return toRegExp().pattern();
		case BUFFER:
			return std::string(toString(), toNumber());
		case NUMBER: {
			char buf[256];
			snprintf(buf, sizeof(buf), "%llu", toNumber());
			return buf;
		}
		case BOOLEAN: {
			static std::string trueStr("true");
			static std::string falseStr("false");
			return toBool() ? trueStr : falseStr;
		}
		case ARRAY:
		case IP:
		case FUNCTION:
		default:
			// illegal/unsupported cast
			return std::string();
	}
}

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

SocketSpec& operator<<(SocketSpec& spec, const FlowParams& params) // {{{ 
{
	// server:
	//   args usage (TCP): 'bind' => address, 'port' => num, ['backlog' => num], ['multi_accept' => num]
	//             (unix): 'bind' => path, ['backlog' => num]
	//
	// client:
	//   args usage (TCP): 'address' => address, 'port' => num
	//             (unix): 'path' => path

	spec.clear();

	unsigned mask = 0;
	enum Item {
		None        = 0x0000,
		Address     = 0x0001,
		Port        = 0x0002,
		Backlog     = 0x0004,
		MultiAccept = 0x0008
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
			if (key == "path") { // client UNIX domain socket
				m = Item::Address;
				if (r[1].isString()) {
					spec = SocketSpec::fromLocal(r[1].toString());
				} else {
					fprintf(stderr, "Invalid UNIX domain socket path specified (must be a path-string).\n");
					goto err;
				}
			} else if (key == "address") { // client TCP/IP
				m = Item::Address;
				if (r[1].isIPAddress()) {
					spec = SocketSpec::fromInet(r[1].toIPAddress(), -1);
				} else {
					fprintf(stderr, "Invalid IP address specified.\n");
					goto err;
				}
			} else if (key == "bind") {
				m = Item::Address;
				if (r[1].isIPAddress()) {
					spec = SocketSpec::fromInet(r[1].toIPAddress(), -1);
				} else if (r[1].isString()) {
					spec = SocketSpec::fromLocal(r[1].toString());
				} else {
					fprintf(stderr, "Invalid bind address specified (must be a path-string or IP address).\n");
					goto err;
				}
			} else if (key == "port") {
				m = Item::Port;
				if (r[1].isNumber())
					spec.setPort(r[1].toNumber());
				else {
					fprintf(stderr, "Invalid port number given (must be a number).\n");
					goto err;
				}
			} else if (key == "backlog") {
				m = Item::Backlog;
				if (r[1].isNumber())
					spec.setBacklog(r[1].toNumber());
				else {
					fprintf(stderr, "Invalid backlog size given (must be a number).\n");
					goto err;
				}
			} else if (key == "multi_accept") {
				m = Item::MultiAccept;
				if (r[1].isNumber()) {
					spec.setMultiAcceptCount(std::max(r[1].toNumber(), 1LL));
				} else {
					fprintf(stderr, "Invalid multi_accept given (must be a number).\n");
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
		fprintf(stderr, "No host address given.\n");
		goto err;
	}

	if (spec.isLocal() && spec.port() >= 0) {
		fprintf(stderr, "Local (unix) sockets have no port numbers.\n");
		goto err;
	}

	return spec;

err:
	spec.clear();
	return spec;
}
// }}}

} // namespace x0
