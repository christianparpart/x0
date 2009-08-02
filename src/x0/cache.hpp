/* <x0/cache.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

/*
 * http://en.wikipedia.org/wiki/Cache_algorithms#Most_Recently_Used
 * http://en.wikipedia.org/wiki/Cache_algorithms
 *
 * TODO Implement something more effective than the current one, 
 *      or even provide different implementations for different goals.
 */

#ifndef sw_x0_cache_hpp
#define sw_x0_cache_hpp

#include <boost/noncopyable.hpp>
#include <cstddef>
#include <vector>
#include <map>

namespace x0 {

template<class Key, class Value>
class cache :
	boost::noncopyable
{
private:
	struct Node
	{
		const Key *key_ptr;
		Value *value_ptr;
		std::size_t cost;
		Node *previous;
		Node *next;

		Node() : key_ptr(0), value_ptr(0), cost(), previous(), next() {}
		Node(Value *value, std::size_t _cost) : key_ptr(0), value_ptr(value), cost(_cost), previous(), next() {}
	};

	typedef typename std::map<Key, Node> hash;

public:
	explicit cache(std::size_t maxcost);
	~cache();

	// method based access
	bool insert(const Key&& key, Value *value, std::size_t cost = 1);
	bool contains(const Key&& key) const;
	Value *get(const Key&& key) const;
	void remove(const Key&& key);
	void clear();

	// index based access
	Value *operator[](const Key&& key) const;

	// function object based acces
	Value *operator()(const Key&& key) const;
	bool operator()(const Key&& key, Value *value, std::size_t cost = 1);

	// properties
	std::size_t max_cost() const;
	void max_cost(std::size_t value);

	std::size_t cost() const;

	std::size_t size() const;
	bool empty() const;
	std::vector<Key> keys() const;

private:
	void trim(std::size_t limit);
	void unlink(Node& n);
	Value *relink(const Key&& n);

private:
	hash hash_;
	Node *first_;
	Node *last_;
	std::size_t max_cost_;
	std::size_t cur_cost_;
};

} // namespace x0

#include <x0/cache.ipp>

#endif
