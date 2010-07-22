/* <x0/Utility.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_utility_hpp
#define sw_x0_utility_hpp (1)

#include <tuple>

// meta programming / template utilities

//! \addtogroup meta_programming
//@{

// --------------------------------------------------------------------
// index_list traits

/** compile-time container of indexes.
 */
template<int...> struct index_list { };

/** index_list_add<List, N>::type
 *
 * meta function that takes an index_list container and adds an element.
 *
 * Input:
 *   - List - the list to add the other index to
 *   - N - the next index to add to the list, OtherList.
 * Output:
 *   - index_list_add<OtherList, N>::type - the resulting index_list
 */
template<typename, int> struct index_list_add;

template<int... List, int N> struct index_list_add<index_list<List...>, N> {
	typedef index_list<List..., N> type;
};

/** build_indexes<N>::type()
 *
 * meta function that builds an index-container from 0 to N - 1.
 *
 * Input:
 *   - N - the number of indices to build from 0 to N minus 1.
 * Output:
 *   - build_indexes<N>::type - the index_list<...> of N indices.
 */
template<int N> struct build_indexes {
	typedef typename index_list_add<typename build_indexes<N - 1>::type, N - 1>::type type;
};

template<> struct build_indexes<0> {
	typedef index_list<> type;
};

// --------------------------------------------------------------------
// functor invocation with tuple driven arguments

/** internal helper function for call_unpacked(). */
template<typename Functor, typename... Args, int... Indexes>
inline void call_impl(Functor functor, std::tuple<Args...>&& args, index_list<Indexes...>)
{
	functor(std::get<Indexes>(std::move(args))...);
}

/** 
 * \brief this function will invoke the passed functor with the given arguments tuple as native arguments passed.
 *
 * \param functor the function to call
 * \param args the tuple of arguments to pass natively to \p functor.
 *
 * \code
 *     call_unpacked(std::sin, std::make_tuple(3.14));
 * \endcode
 */
template<typename Functor, typename... Args>
inline void call_unpacked(Functor functor, std::tuple<Args...>&& args)
{
	call_impl(functor, std::move(args), typename build_indexes<sizeof...(Args)>::type());
}

//@}

#endif
