#ifndef SlidingArray_C_included
#define SlidingArray_C_included

#include "SlidingArray.h"

#include <assert.h>
#include <stddef.h>

template <class T>
SlidingArray<T>::SlidingArray(unsigned int n)
    : p(NULL), offset(0), size(0), nalloc(0), zero(0)
{
    if (n)
    {	nalloc = n;
	p = new T[n];
    }
}

template <class T>
SlidingArray<T>::~SlidingArray()
{
    delete [] p;
}

template <class T>
T&
SlidingArray<T>::operator [] (signed int index)
{
    assert(size <= nalloc);
    if (!size)
	offset = index;
    index -= offset;

    if (index < 0)
    {
	//  Oops.  Grow the array backwards.

	while (size > 0 && p[size - 1] == 0)
	    --size;
	if (size - index > nalloc)
	{
	    //  Reallocate.

	    nalloc = size - index + 1;
	    T *np = new T[nalloc], *s, *d, *e;
	    for (d = np, e = np - index; d < e; *d++ = 0)
		continue;
	    for (s = p, e = p + size; s < e; *d++ = *s++)
		continue;
	    delete [] p;
	    p = np;
	    offset += index;		// index < 0
	    size -= index;
	    index = 0;
	}
	else
	{
	    //  Slide right.

	    T *s, *d, *e;
	    for (e = p, s = e + size, d = s - index; s > e; *--d = *--s)
		continue;
	    for ( ; d > e; *--d = 0)
		continue;
	    offset += index;		//  index < 0
	    size -= index;
	    index = 0;

	}
    }
    else if (index >= size)
    {
	//  Grow the array forwards.

	if (index >= nalloc)
	{
	    //  How far left can we shift?

	    int nzl;
	    for (nzl = 0; nzl < size && p[nzl] == 0; nzl++)
		continue;
	    if (index - nzl >= nalloc)
	    {
		//  Reallocate.

		assert(nalloc < index + 10);
		nalloc = index + 10;
		T *np = new T[nalloc];
		for (T *d = np, *s = p + nzl, *e = p + size; s < e; )
		    *d++ = *s++;
		delete [] p;
		p = np;
	    }
	    else
	    {
		//  Slide left.

		for (T *d = p, *s = p + nzl, *e = p + size; s < e; *d++ = *s++)
		    continue;
	    }
	    offset += nzl;
	    index -= nzl;
	    size -= nzl;
	}
	assert(index < nalloc);
	while (size <= index)
	    p[size++] = 0;
    }
    assert(0 <= index);
    assert(index < size);
    assert(size <= nalloc);
    return p[index];
}

#ifdef UNIT_TEST_SlidingArray

#include <stdio.h>

int main()
{
    typedef SlidingArray<float> WaterSlide;
    WaterSlide wheee;
    const WaterSlide& cwsr = wheee;
    wheee[1000] += 5.5;
    wheee[1001] -= 6.6;
    assert(wheee[999] == 0);
    assert(wheee[1000] == 5.5);
    wheee[1000] -= 5.5;
    assert(wheee[1000] == 0);
    wheee[1001] = 0;
    int i;
    for (i = 1002; i < 1020; i++)
    {   if (i == 1012)
	{   float f = wheee[999];
	    f += f;
	    assert(f == 0);
	    wheee[1002] = 0;
	    wheee[1003] = 0;
	}
	wheee[i] = i / 8.0;
	for (int j = 998; j < 1030; j++)
	{   if (j < (i < 1012 ? 1002 : 1004) || j > i)
		assert(cwsr[j] == 0);
	    else
		assert(cwsr[j] == j / 8.0);
	}
    }
    assert(wheee.low_index() == 1004);
    assert(wheee.high_index() == 1020);
    for (i = wheee.low_index(); i < wheee.high_index(); i++)
    {
	float f = wheee[i];
	if (i == 1002 || i == 1003)
	    assert(f == 0);
	else
	    assert(f == i / 8.0);
    }
    assert(wheee.low_index() == 1004);
    assert(wheee.high_index() == 1020);
    return 0;
}

#endif /* UNIT_TEST_SlidingArray */

#endif /* !SlidingArray_C_included */
