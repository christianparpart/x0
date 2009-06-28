#ifndef x0_range_def_ipp
#define x0_range_def_ipp

#include <boost/tokenizer.hpp>

namespace x0 {

inline range_def::range_def() : ranges_()
{
}

inline range_def::range_def(const std::string& spec) : ranges_()
{
	parse(spec);
}

inline void range_def::parse(const std::string& value)
{
	// ranges-specifier = byte-ranges-specifier
	// byte-ranges-specifier = bytes-unit "=" byte-range-set
	// byte-range-set  = 1#( byte-range-spec | suffix-byte-range-spec )
	// byte-range-spec = first-byte-pos "-" [last-byte-pos]
	// first-byte-pos  = 1*DIGIT
	// last-byte-pos   = 1*DIGIT

	// TODO
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	tokenizer spec(value, boost::char_separator<char>(" \t"));
	tokenizer::iterator si(spec.begin());
	if (si != spec.end() && *si == "byte")
	{
		std::string brange(*++si);
	}
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
