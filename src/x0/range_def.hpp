#ifndef x0_range_def_hpp
#define x0_range_def_hpp

#include <x0/property.hpp>
#include <x0/types.hpp>

namespace x0 {

/** 
 * \ingroup core
 * \brief represents a Range-header field with high-level access.
 */
class range_def
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
	range_def();
	explicit range_def(const std::string& spec);

	value_property<std::string> unit_name;

	bool parse(const std::string& value);

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

} // namespace x0

#include <x0/range_def.ipp>

#endif
