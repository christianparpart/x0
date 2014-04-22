/* <x0/ternary_search.hpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_trie_h
#define x0_trie_H

#include <x0/Api.h>
#include <stdexcept>
#include <string>
#include <cstring>

namespace x0 {

//! \addtogroup base
//@{

/**
 * \brief a generic <b>ternary search trie</b> data structure
 *
 * \see http://en.wikipedia.org/wiki/Ternary_search
 * \see http://en.wikipedia.org/wiki/Divide_and_conquer_algorithm
 * \see http://en.wikipedia.org/wiki/Trie
 * \see http://www.cs.bu.edu/teaching/c/tree/ternary_search/
 */
template<typename _Key, typename _Value>
class ternary_search
{
public:
    typedef _Key key_type;
    typedef typename key_type::value_type keyelem_type;
    typedef _Value value_type;

private:
    static const keyelem_type EOS = 0;

    struct node // {{{
    {
        keyelem_type value;
        value_type data;

        node *left;
        node *middle;
        node *right;

        node() :
            value(), data(), left(0), middle(0), right(0)
        {
        }

        ~node()
        {
            left = middle = right = 0;
        }
    }; // }}}

    struct nodelines // {{{
    {
        node *line;
        nodelines *next;

        nodelines() :
            line(0), next(0)
        {
        }

        ~nodelines()
        {
        }
    }; // }}}

    class node_iterator // {{{
    {
    private:
        node *np_;

    public:
        node_iterator() :
            np_(0)
        {
        }

        node_iterator(node *np) :
            np_(np)
        {
        }

        operator bool () const
        {
            return np_ != NULL;
        }

        const value_type& operator*() const
        {
            if (!np_)
                throw std::runtime_error("ternary_search's iterator value is NULL");

            return np_->data;
        }

        friend bool operator!=(const node_iterator& a, const node_iterator& b)
        {
            return a.np_ != b.np_;
        }

        friend bool operator==(const node_iterator& a, const node_iterator& b)
        {
            return a.np_ == b.np_;
        }

        node_iterator& operator++()
        {
            throw std::runtime_error("ternary_search::iterator::operator++() not yet implemented");

            return *this;
        }
    };
    // }}}

public:
    typedef node_iterator iterator;

public:
    explicit ternary_search(int width = 30) :
        width_(width),
        lines_(0),
        freelist_(),
        size_(0)
    {
        std::memset(head_, 0, sizeof(head_));

        lines_ = new nodelines;
        lines_->line = new node[width_];
        lines_->next = NULL;

        node *current_node = lines_->line;
        freelist_ = current_node;

        for (int i = 1; i < width_; ++i)
        {
            current_node->middle = &(lines_->line[i]);
            current_node = current_node->middle;
        }
        current_node->middle = NULL;

        /* lines->line now points to an array of nodes of width_ size.
         * the freelist_ field points to the first entry at lines->line.
         * each node's middle attribute within this "line" points to its right neighbour
         * and the last points to nullptr.
         */
    }

    ~ternary_search()
    {
        nodelines *next_line = lines_;

        do
        {
            nodelines *current_line = next_line;
            next_line = current_line->next;
            delete[] current_line->line;
            delete current_line;
        }
        while (next_line != NULL);
    }

    /**
     * clears out all datasets in this container.
     * \see insert(), remove()
     */
    void clear()
    {
        throw std::runtime_error("ternary_search::clear() not yet implemented");
    }

    /**
     * adds a new dataset item pair into this container.
     *
     * \param key the key to use.
     * \param value the value data to use.
     * \param replace true if possible already existing match is to be overwritten, false (default) otherwise.
     * \see find(), erase(), remove(), clear()
     */
    void insert(const key_type& key, const value_type& value, bool replace = false) // {{{
    {
        node *current_node;
        node *new_node_tree_begin = NULL;
        bool perform_loop = true;

        if (key.empty())
            throw std::runtime_error("ternary_search: key may not be empty");

        if (head_[int(key[0])] == NULL)
        {
            current_node = head_[int(key[0])] = acquire_free_node(key[1]);

            if (key[1] == EOS)
            {
                current_node->data = value;
                ++size_;
                return;
            }
            else
            {
                perform_loop = false;
            }
        }

        current_node = head_[int(key[0])];
        int key_index = 1;

        while (perform_loop)
        {
            if (key[key_index] == current_node->value)
            {
                if (key[key_index] == EOS)
                {
                    if (!replace) throw std::runtime_error("ternary_search: duplicate key");

                    current_node->data = value;
                    ++size_;
                    return;
                }
                else
                {
                    if (current_node->middle == NULL)
                    {
                        current_node->middle = acquire_free_node(key[key_index]);
                        new_node_tree_begin = current_node;
                        current_node = current_node->middle;
                        break;
                    }
                    else
                    {
                        current_node = current_node->middle;
                        ++key_index;
                        continue;
                    }
                }
            }

            if ((current_node->value == 0 && key[key_index] < 64) ||
                (current_node->value != 0 && key[key_index] < current_node->value))
            {
                if (current_node->left == NULL)
                {
                    current_node->left = acquire_free_node(key[key_index]);
                    new_node_tree_begin = current_node;
                    current_node = current_node->left;

                    if (key[key_index] == EOS)
                    {
                        current_node->data = value;
                        ++size_;
                        return;
                    }
                    break;
                }
                else
                {
                    current_node = current_node->left;
                    continue;
                }
            }
            else
            {
                if (current_node->right == NULL)
                {
                    current_node->right = acquire_free_node(key[key_index]);
                    new_node_tree_begin = current_node;
                    current_node = current_node->right;
                    break;
                }
                else
                {
                    current_node = current_node->right;
                    continue;
                }
            }
        }

        do
        {
            ++key_index;

            try
            {
                current_node->middle = acquire_free_node(key[key_index]);
                current_node = current_node->middle;
            }
            catch (...) // XXX ideally only memory allocation error
            {
                // put allocated nodes back to freelist and rethrow.
                current_node = new_node_tree_begin->middle;

                while (current_node->middle != NULL)
                    current_node = current_node->middle;

                current_node->middle = freelist_;
                freelist_ = new_node_tree_begin->middle;
                new_node_tree_begin->middle = NULL;

                throw;
            }
        }
        while (key[key_index] != EOS);

        current_node->data = value;
        ++size_;
    }
    // }}}

    /**
     * searches for a given key in this container.
     *
     * \param key the pattern to lookup and retrieve its data value for.
     * \param prefix_len an optional out arg which will contain the number of prefix elements having traversed.
     * \returns an iterator referencing the resulting data value or end() if not found.
     * \see begin(), end()
     */
    node_iterator find(const key_type& key, int *prefix_len = NULL) const // {{{
    {
        node *longest_match = NULL;

        if (key.empty())
            return end();

        if (head_[int(key[0])] == 0)
            return end();

        if (prefix_len) *prefix_len = 0;
        node *current_node = head_[int(key[0])];
        int key_index = 1;

        while (current_node != NULL)
        {
            if (key[key_index] == current_node->value)
            {
                if (current_node->value == 0)
                {
                    if (prefix_len) *prefix_len = key_index;
                    return node_iterator(current_node);
                }
                else
                {
                    current_node = current_node->middle;
                    if (current_node && current_node->value == 0)
                    {
                        if (prefix_len) *prefix_len = key_index + 1;
                        longest_match = current_node;
                    }
                    ++key_index;
                    continue;
                }
            }
            else if ((current_node->value == 0 && key[key_index] < 64) ||
                     (current_node->value != 0 && key[key_index] < current_node->value))
            {
                if (current_node->value == 0)
                {
                    if (prefix_len) *prefix_len = key_index;
                    longest_match = current_node;
                }
                current_node = current_node->left;
                continue;
            }
            else
            {
                if (current_node->value == 0)
                {
                    if (prefix_len) *prefix_len = key_index;
                    longest_match = current_node;
                }
                current_node = current_node->right;
                continue;
            }
        }
        return node_iterator(longest_match);
    }
    // }}}

    /**
     * removes a dataset item at given iterator location.
     *
     * after this operation, the passed iterator points automatically to the next dataset if available, or end().
     *
     * \see find(), end(), remove(), remove_all()
     */
    void erase(iterator iter)
    {
        throw std::runtime_error("ternary_search::erase() not yet implemented");
    }

    /** removes a dataset item from this container */
    void remove(const key_type& key)
    {
        // TODO
        throw std::runtime_error("ternary_search::remove() not yet implemented");
    }

    /** removes all dataset items that match given key (as found by lookup) */
    void remove_all(const key_type& key)
    {
        while (iterator i = find(key))
            erase(i);
    }

    /**
     * retrieves the beginning of the data items being stored in this container.
     */
    node_iterator begin() const
    {
        throw std::runtime_error("ternary_search::begin() not yet implemented");
    }

    /**
     * retrieves the end-marker that denotes the end of iteration for this container.
     */
    node_iterator end() const
    {
        return node_iterator();
    }

    /** tests wether given key is stored in this container. */
    inline bool contains(const key_type& key) const
    {
        return find(key) != end();
    }

    /** retrieves the number of stored items in this container. */
    inline std::size_t size() const
    {
        return size_;
    }

    /** tests wether given dataset container is empty or not. */
    inline bool empty() const
    {
        return !size_;
    }

    const value_type& operator[](const key_type& key) const
    {
        return get(key, NULL);
    }

    value_type& operator[](const key_type& key)
    {
        if (!contains(key))
            const_cast<ternary_search *>(this)->insert(key, value_type());

        return *const_cast<value_type *>(&get(key, NULL));
    }

private:
    int width_;
    nodelines *lines_;
    node *freelist_;
    node *head_[127];

    std::size_t size_;

private:
    inline node *acquire_free_node(const keyelem_type& value)
    {
        if (freelist_ == NULL)
            grow_node_free_list();

        node *n = freelist_;
        freelist_ = freelist_->middle;

        n->middle = NULL;
        n->value = value;

        return n;
    }

    inline void grow_node_free_list()
    {
        node *current_node;
        nodelines *new_line;
        int i;

        new_line = new nodelines;
        new_line->line = new node[width_];
        new_line->next = lines_;
        lines_ = new_line;

        current_node = lines_->line;
        freelist_ = current_node;

        for (i = 1; i < width_; ++i)
        {
            current_node->middle = &(lines_->line[i]);
            current_node = current_node->middle;
        }
        current_node->middle = NULL;
    }
};

//@}

} // namespace x0

#endif
