/*
 * FILE: eoe/cmd/miser/cmd/miser/iterator_ext.h
 *
 * DESCRIPTION:
 *      Extention to iterator STL library.
 */

/*************************************************************************
 *                                                                       *
 * Copyright (c) 1994                                                    *
 * Hewlett-Packard Company                                               *
 *                                                                       *
 * Permission to use, copy, modify, distribute and sell this software    * 
 * and its documentation for any purpose is hereby granted without fee,  * 
 * provided that the above copyright notice appear in all copies and     *
 * that both that copyright notice and this permission notice appear     *
 * in supporting documentation.  Hewlett-Packard Company makes no        *
 * representations about the suitability of this software for any        *
 * purpose.  It is provided "as is" without express or implied warranty. *
 *                                                                       *
 *************************************************************************/


#ifndef __ITERATOR_EXT_H
#define __ITERATOR_EXT_H

template <class Container>
struct 
cyclic_iterator : random_access_iterator<Container::value_type,
	Container::difference_type>
{
	Container* container;
	Container::iterator i;

	cyclic_iterator(Container& x, Container::iterator I) 
		: container(&x), i(I) {}

	cyclic_iterator<Container>& operator+(int n) const {

		if (i - container->begin() + n >= container->size()) {
			return cyclic_iterator<Container>(*container, 
				i + n - container->size());
		} else {
			return cyclic_iterator<Container>(*container, i + n);
		}
	}

	cyclic_iterator<Container>& operator++() {

		if (++i == container->end()) {
			i = container->begin();
		}

		return *this;
	}

	cyclic_iterator<Container>& operator++(int) {
		cyclic_iterator tmp = *this;

		if (++i == container->end()) {
			i = container->begin();
		}

		return tmp;
	}

	cyclic_iterator<Container>& operator--() {

		if (i == container->begin()) {
			i = container->end();
		}

		--i;
		int q = i - container->begin();

		return *this;
	}

	cyclic_iterator<Container>& operator--(int) {
		cyclic_iterator tmp = *this;

		if (i == container->begin()) {
			i = container->end();
		}

		--i;
		int s = i - container->begin();	

		return tmp;
	}

	bool operator==(const cyclic_iterator<Container>& cmp) const {
		return cmp.i == i;
	}

	Container::difference_type operator-(const 
			cyclic_iterator<Container>& cmp) const {
		Container::difference_type diff = i - cmp.i;
		Container::difference_type max  = container->size() / 2;

		int retval = (diff <= -max)
			? diff + container->size() : (diff > max)
			? diff - container->size() : diff;

		return retval;
	}

	Container::reference operator*() { return *i;}

}; /* cyclic_iterator */


template <class Container>
get_offset(cyclic_iterator<Container> iter)
{
	return iter.i - iter.container->begin();

} /* get_offset */


template <class Container, class Iterator>
cyclic_iterator<Container> 
cycler(Container& x, Iterator i) 
{
	return cyclic_iterator<Container>(x, Container::iterator(i));

} /* cycler */


#endif /* __ITERATOR_EXT_H */ 
