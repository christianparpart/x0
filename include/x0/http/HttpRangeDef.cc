/* <HttpRangeDef.cc>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef x0_HttpRangeDef_ipp
#define x0_HttpRangeDef_ipp

// XXX http://tools.ietf.org/html/draft-fielding-http-p5-range-00

#include <boost/tokenizer.hpp>
#include <cstdlib>

namespace x0 {

inline HttpRangeDef::HttpRangeDef() : ranges_()
{
}

inline HttpRangeDef::HttpRangeDef(const BufferRef& spec) : ranges_()
{
	parse(spec);
}

/**
 * parses an HTTP/1.1 conform Range header \p value.
 * \param value the HTTP header field value retrieved from the Range header field. 
 *
 * The following ranges can be specified:
 * <ul>
 *    <li>explicit range, from \em first to \em last (first-last)</li>
 *    <li>explicit begin to the end of the entity (first-)</li>
 *    <li>the last N units of the entity (-last)</li>
 * </ul>
 */
inline bool HttpRangeDef::parse(const BufferRef& value)
{
	// ranges-specifier = byte-ranges-specifier
	// byte-ranges-specifier = bytes-unit "=" byte-range-set
	// byte-range-set  = 1#( byte-range-spec | suffix-byte-range-spec )
	// byte-range-spec = first-byte-pos "-" [last-byte-pos]
	// first-byte-pos  = 1*DIGIT
	// last-byte-pos   = 1*DIGIT

	// suffix-byte-range-spec = "-" suffix-length
	// suffix-length = 1*DIGIT

	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

	tokenizer spec(value, boost::char_separator<char>("="));
	tokenizer::iterator si(spec.begin());

	if (si != spec.end())
	{
		unitName = *si;

		if (unitName() == "bytes")
		{
			std::string brange(*++si);
			tokenizer t2(brange, boost::char_separator<char>(","));

			for (tokenizer::iterator i = t2.begin(), e = t2.end(); i != e; ++i)
			{
				if (!parseRangeSpec(*i))
					return false;
			}
		}
	}
	return true;
}

inline bool HttpRangeDef::parseRangeSpec(const std::string& spec)
{
	std::size_t a, b;
	char *p = const_cast<char *>(spec.c_str());

	// parse first element
	if (std::isdigit(*p))
	{
		a = strtoul(p, &p, 10);
	}
	else
	{
		a = npos;
	}

	if (*p != '-')
	{
		printf("parse error: %s (%s)\n", p, spec.c_str());
		return false;
	}

	++p;

	// parse second element
	if (std::isdigit(*p))
	{
		b = strtoul(p, &p, 10);
	}
	else
	{
		b = npos;
	}

	if (*p != '\0') // garbage at the end
		return false;

	ranges_.push_back(std::make_pair(a, b));

	return true;
}

inline void HttpRangeDef::push_back(std::size_t offset1, std::size_t offset2)
{
	ranges_.push_back(std::make_pair(offset1, offset2));
}

inline void HttpRangeDef::push_back(const std::pair<std::size_t, std::size_t>& range)
{
	ranges_.push_back(range);
}

inline std::size_t HttpRangeDef::size() const
{
	return ranges_.size();
}

inline bool HttpRangeDef::empty() const
{
	return !ranges_.size();
}

inline const HttpRangeDef::element_type& HttpRangeDef::operator[](std::size_t index) const
{
	return ranges_[index];
}

inline HttpRangeDef::iterator HttpRangeDef::begin()
{
	return ranges_.begin();
}

inline HttpRangeDef::iterator HttpRangeDef::end()
{
	return ranges_.end();
}

inline HttpRangeDef::const_iterator HttpRangeDef::begin() const
{
	return ranges_.begin();
}

inline HttpRangeDef::const_iterator HttpRangeDef::end() const
{
	return ranges_.end();
}

inline std::string HttpRangeDef::str() const
{
	std::stringstream sstr;
	int count = 0;

	sstr << unitName();

	for (const_iterator i = begin(), e = end(); i != e; ++i)
	{
		if (count++)
		{
			sstr << ", ";
		}

		if (i->first != npos)
		{
			sstr << i->first;
		}

		sstr << '-';

		if (i->second != npos)
		{
			sstr << i->second;
		}
	}

	return sstr.str();
}

} // namespace x0

// vim:syntax=cpp
#endif
