#ifndef x0_range_def_ipp
#define x0_range_def_ipp

// XXX http://tools.ietf.org/html/draft-fielding-http-p5-range-00

#include <boost/tokenizer.hpp>
#include <cstdlib>

namespace x0 {

inline range_def::range_def() : ranges_()
{
}

inline range_def::range_def(const std::string& spec) : ranges_()
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
inline bool range_def::parse(const std::string& value)
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
		unit_name = *si;

		if (unit_name() == "bytes")
		{
			std::string brange(*++si);
			tokenizer t2(brange, boost::char_separator<char>(","));

			for (tokenizer::iterator i = t2.begin(), e = t2.end(); i != e; ++i)
			{
				if (!parse_range_spec(*i))
					return false;
			}
		}
	}
	return true;
}

inline bool range_def::parse_range_spec(const std::string& spec)
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

inline void range_def::push_back(std::size_t offset1, std::size_t offset2)
{
	ranges_.push_back(std::make_pair(offset1, offset2));
}

inline void range_def::push_back(const std::pair<std::size_t, std::size_t>& range)
{
	ranges_.push_back(range);
}

inline std::size_t range_def::size() const
{
	return ranges_.size();
}

inline bool range_def::empty() const
{
	return !ranges_.size();
}

inline const range_def::element_type& range_def::operator[](std::size_t index) const
{
	return ranges_[index];
}

inline range_def::iterator range_def::begin()
{
	return ranges_.begin();
}

inline range_def::iterator range_def::end()
{
	return ranges_.end();
}

inline range_def::const_iterator range_def::begin() const
{
	return ranges_.begin();
}

inline range_def::const_iterator range_def::end() const
{
	return ranges_.end();
}

inline std::string range_def::str() const
{
	std::stringstream sstr;
	int count = 0;

	sstr << unit_name();

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
