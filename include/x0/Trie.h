/* <x0/Trie.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef x0_trie_h
#define x0_trie_H

#include <x0/Api.h>
#include <stdexcept>
#include <string>
#include <iostream>

namespace x0 {

//! \addtogroup base
//@{

/**
 * \brief a generic trie data structure
 *
 * \see http://en.wikipedia.org/wiki/Trie
 * \see http://www.cs.bu.edu/teaching/c/tree/trie/
 * \see http://paste.lisp.org/display/12161 (trie in C#, suffix trie)
 */
template<typename _Key, typename _Value>
class X0_API trie
{
public:
	typedef _Key key_type;
	typedef typename key_type::value_type keyelem_type;
	typedef _Value value_type;

private:
	struct node
	{
		keyelem_type key;
		value_type value;

		node *next;
		node *children;

		/** searches for a node with given key */
		node *find(keyelem_type key) const
		{
			for (auto cur = this; cur != 0; cur = cur->next)
				if (cur->key == key)
					return (node*) cur;

			return 0;
		}

		node() :
			key(), value(), next(0), children(0)
		{
		}

		node(keyelem_type key, node *next) :
			key(key), value(), next(next), children(0)
		{
		}

		~node()
		{
			delete next;
			delete children;
			next = children = 0;
		}
	};

	struct node_iterator // {{{
	{
		node *np_;

		node_iterator() :
			np_(0)
		{
		}

		explicit node_iterator(node *np) :
			np_(np)
		{
		}

		node_iterator(const node_iterator& v) :
			np_(v.np_)
		{
		}

		value_type& operator*()
		{
			return np_->value;
		}

		const value_type& operator*() const
		{
			return np_->value;
		}

		node_iterator& operator++()
		{
			return *this;
		}

		friend bool operator==(const node_iterator& a, const node_iterator& b)
		{
			return a.np_ == b.np_;
		}

		friend bool operator!=(const node_iterator& a, const node_iterator& b)
		{
			return a.np_ != b.np_;
		}
	}; // }}}

public:
	typedef node_iterator iterator;
	typedef const node_iterator const_iterator;

public:
	trie() :
		root_(),
		size_()
	{
	}

	~trie()
	{
	}

	const value_type& operator[](const key_type& key) const
	{
		const_iterator i(find(key));

		if (i != end())
			return *i;

		throw std::runtime_error("key not found in trie");
	}

	value_type& operator[](const key_type& key)
	{
		return at(key);
	}

	value_type& at(const key_type& key)
	{
		iterator i(find(key));
		if (i != end()) return *i;

		i = insert(key, value_type());
		return *i;
	}

	std::size_t size() const
	{
		return size_;
	}

	iterator begin() const
	{
		throw std::runtime_error("not implemented");
	}

	iterator end() const
	{
		return iterator();
	}

	iterator insert(const key_type& key, const value_type& value)
	{
		std::cout << "trie.insert(" << key << ", " << value << ")" << std::endl;
		node *level = &root_;

		int n = 0;
		for (auto i = key.begin(), e = key.end(); i != e; ++i)
		{
			std::cout << "    acquire at level " << n++ << ", elem " << *i << std::endl;
			level = acquire(*i, &level->children);
		}

		std::cout << "    set value" << std::endl;
		++size_;
		level->value = value;

		return iterator(level);
	}

	void erase(iterator it)
	{
		throw std::runtime_error("not implemented");
	}

	bool contains(const key_type& key) const
	{
		return contains(key.c_str());
	}

	bool contains(const keyelem_type *key) const
	{
		return find(key) != end();
	}

	iterator find(const key_type& key) const
	{
		return find(key.c_str());
	}

	iterator find(const keyelem_type *key) const
	{
		std::cout << "trie.find(" << key << ")" << std::endl;

		const node* level = &root_;
		const keyelem_type* kp = key;

		while (node *found = level->children->find(*kp)) {
			std::cout << "  lookup(" << ((char)(*kp ? *kp : '$')) << ")" << std::endl;
			if (*kp++ == '\0')
				return iterator(found);

			level = found;
		}

		if (*kp == '\0')
			return iterator((node*) level);

//		if (level->children == 0)
//			return iterator(level);

		return end();
	}

private:
	node root_;
	std::size_t size_;

private:
	/**
	 * searches for given key and retrieves its node, but if not found, create a new node.
	 *
	 * \note this is not a member function of \c node because we need a pointer to pointer ref.
	 */
	node *acquire(keyelem_type key, node ** level)
	{
		// no nodes stored at `level` yet. so initialize with first node in line.
		if (!level)
			return *level = new node(key, *level);

		for (auto p = level; *p != 0; p = &(*p)->next)
			if ((*p)->key == key)
				return *p;

		//std::cout << "        create node(" << key << ")" << std::endl;
		return *level = new node(key, *level);
	}
};

//@}

} // namespace x0

#endif
