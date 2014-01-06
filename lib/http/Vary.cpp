#include <x0/http/Vary.h>
#include <x0/http/HttpRequest.h>
#include <x0/Tokenizer.h>

using namespace x0;

/*
 * net read request
 * parse request, populate request struct
 * generate cache key
 *
 */

namespace x0 {

Vary::Vary(size_t count) :
	names_(count),
	values_(count)
{
}

Vary::~Vary()
{
}

std::unique_ptr<Vary> Vary::create(const x0::HttpRequest* r)
{
	return create(BufferRef(r->responseHeaders["Vary"]), r->requestHeaders);
}

VaryMatch Vary::match(const Vary& other) const
{
	if (size() != other.size())
		return VaryMatch::None;

	for (size_t i = 0, e = size(); i != e; ++i) {
		if (names_[i] != other.names_[i])
			return VaryMatch::None;

		if (names_[i] != other.names_[i])
			return VaryMatch::ValuesDiffer;
	}

	return VaryMatch::Equals;
}

VaryMatch Vary::match(const x0::HttpRequest* r) const
{
	for (size_t i = 0, e = size(); i != e; ++i) {
		const auto& name = names_[i];
		const auto& value = values_[i];
		const auto otherValue = r->requestHeader(name);

		if (otherValue != value) {
			return VaryMatch::ValuesDiffer;
		}
	}

	return VaryMatch::Equals;
}

} // namespace x0
