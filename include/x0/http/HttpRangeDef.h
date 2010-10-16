/* <x0/HttpRangeDef.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef x0_HttpRangeDef_h
#define x0_HttpRangeDef_h

#include <x0/Property.h>
#include <x0/Buffer.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <vector>
#include <sstream>

namespace x0 {

//! \addtogroup core
//@{

/** 
 * \brief represents a Range-header field with high-level access.
 */
class HttpRangeDef
{
public:
	typedef std::pair<std::size_t, std::size_t> element_type;

	/** internally used range vector type. */
	typedef std::vector<element_type> vector_type;

	/** range iterator. */
	typedef vector_type::iterator iterator;

	typedef vector_type::const_iterator const_iterator;

	/** represents an unspecified range item of a range-pair component.
	 *
	 * Example ranges for file of 1000 bytes:
	 * <ul>
	 *   <li>(npos, 500) - last 500 bytes</li>
	 *   <li>(9500, 999) - from 9500 to 999 (also: last 500 bytes in this case)</li>
	 *   <li>(9500, npos) - bytes from 9500 to the end of entity. (also: last 500 bytes in this case)</li>
	 * <ul>
	 */
	static const std::size_t npos = std::size_t(-1);

private:
	vector_type ranges_;

public:
	HttpRangeDef();
	explicit HttpRangeDef(const BufferRef& spec);

	value_property<std::string> unit_name;

	bool parse(const BufferRef& value);

	/// pushes a new range to the list of ranges
	void push_back(std::size_t offset1, std::size_t offset2);

	/// pushes a new range to the list of ranges
	void push_back(const std::pair<std::size_t, std::size_t>& range);

	std::size_t size() const;

	bool empty() const;

	/** retrieves the range element at given \p index. */
	const element_type& operator[](std::size_t index) const;

	/** iterator pointing to the first range element. */
	const_iterator begin() const;

	/** iterator representing the end of this range vector. */
	const_iterator end() const;

	/** iterator pointing to the first range element. */
	iterator begin();

	/** iterator representing the end of this range vector. */
	iterator end();

	/** retrieves string representation of this range. */
	std::string str() const;

private:
	inline bool parse_range_spec(const std::string& spec);
};

//@}

} // namespace x0

#include <x0/http/HttpRangeDef.cc>

#endif
