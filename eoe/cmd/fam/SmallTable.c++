#ifndef SmallTable_C_included
#define SmallTable_C_included

#include "SmallTable.h"

#include <assert.h>

template <class Tkey, class Tvalue>
SmallTable<Tkey, Tvalue>::~SmallTable()
{
    delete [] table;
}

template <class Tkey, class Tvalue>
SmallTable<Tkey, Tvalue>::Closure
SmallTable<Tkey, Tvalue>::position(const Tkey& key) const
{
    unsigned l = 0, r = n;
    while (l < r)
    {
	unsigned mid = (l + r) / 2;
	if (key < table[mid].key)
	    r = mid;
	else if (key > table[mid].key)
	    l = mid + 1;
	else // (key == table[mid].key)
	    return Closure(Closure::PRESENT, mid);
    }
    return Closure(Closure::ABSENT, l);
}

template <class Tkey, class Tvalue>
void
SmallTable<Tkey, Tvalue>::insert(const Tkey& key, const Tvalue& value)
{
    Closure c = position(key);

    if (c.presence == Closure::PRESENT)
    {   table[c.index].value = value;
	return;
    }

    if (n >= nalloc)
    {
	//  Grow the table.

	nalloc = 3 * n / 2 + 10;
	Pair *nt = new Pair[nalloc];
	unsigned i;
	for (i = 0; i < c.index; i++)
	    nt[i] = table[i];
	for ( ; i < n; i++)
	    nt[i + 1] = table[i];
	delete [] table;
	table = nt;
    }
    else
    {
	// Shift following entries right.

	for (unsigned i = n; i > c.index; --i)
	    table[i] = table[i - 1];
    }
    table[c.index].key = key;
    table[c.index].value = value;
    n++;
}

template <class Tkey, class Tvalue>
void
SmallTable<Tkey, Tvalue>::remove(const Tkey& key)
{
    Closure c = position(key);
    assert(c.presence == Closure::PRESENT);

    // Shift following entries left.

    for (unsigned i = c.index + 1; i < n; i++)
	table[i - 1] = table[i];
    --n;

    // Shrink the table.

    if (2 * n < nalloc && n < nalloc - 10)
    {
	nalloc = n;
	Pair *nt = new Pair[nalloc];
	for (unsigned i = 0; i < n; i++)
	    nt[i] = table[i];
	delete [] table;
	table = nt;
    }
}

#ifdef UNIT_TEST_SmallTable

#include <stdio.h>

main()
{
    typedef SmallTable<char, float> TVStand;
    TVStand a;
    a.insert('b', 1.23);
    a.insert('c', 2.34);
    a.insert('a', 0.12);
    a.remove('b');

    assert(a.find('a') == (float) 0.12);
    assert(a.find('b') == 0);
    assert(a.find('c') == (float) 2.34);
    assert(a.find('d') == 0);

    return 0;
}

#endif /* UNIT_TEST_SmallTable */

#endif /* !SmallTable_C_included */
