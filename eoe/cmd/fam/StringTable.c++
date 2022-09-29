#ifndef StringTable_C_included
#define StringTable_C_included

#include "StringTable.h"

#include <assert.h>
#include <string.h>

template <class Tv>
StringTable<Tv>::~StringTable()
{
    {
	for (unsigned i = 0; i < n; i++)
	    delete [] table[i].key;
    }
    delete table;

    table = NULL;
    nalloc = n = 0;
}

template <class Tv>
StringTable<Tv>&
StringTable<Tv>::operator = (const StringTable& that)
{
    if (this != &that)
    {
	unsigned i;
	for (i = 0; i < n; i++)
	    delete [] table[i].key;
	delete table;
	nalloc = n = that.n;
	table = new Pair[nalloc];
	for (i = 0; i < n; i++)
	{   const char *key = that.table[i].key;
	    table[i].key = strcpy(new char[strlen(key) + 1], key);
	    table[i].value = that.table[i].value;
	}
    }
    return *this;
}

template <class Tv>
signed int
StringTable<Tv>::position(const Tk key) const
{
    for (unsigned i = 0; i < n; i++)
	if (!strcmp(key, table[i].key))
	    return i;
    return -1;				// Not found.
}

template <class Tv>
void
StringTable<Tv>::insert(const Tk key, const Tv& value)
{
    signed int index = position(key);
    if (index >= 0)
    {   table[index].value = value;
	return;
    }

    if (n >= nalloc)
    {
	//  Grow the table.

	nalloc = 3 * n / 2 + 10;
	Pair *nt = new Pair[nalloc];
	for (unsigned i = 0; i < n; i++)
	    nt[i] = table[i];
	delete [] table;
	table = nt;
	assert(n < nalloc);
    }
    table[n].key = strcpy(new char[strlen(key) + 1], key);
    table[n].value = value;
    n++;
}

template <class Tv>
void
StringTable<Tv>::remove(const Tk key)
{
    signed int index = position(key);
    assert(index >= 0 && index < n);

    // Delete the matching key.

    delete [] table[index].key;
    table[index] = table[--n];

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

#ifdef UNIT_TEST_StringTable

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

typedef StringTable<float> STF;

main()
{
    STF a;
    static char *numbers[] = {
	"1.01",
	"2.02",
	"3.03",
	"4.04",
	"5.05",
	"6.06",
	"7.07",
	"8.08",
	"9.09",
	"10.10",
	"11.11",
	"12.12",
	"13.13",
	"14.14",
	"15.15",
	NULL
    };
    char **p;
    for (p = numbers; *p; p++)
	a.insert(*p, atof(*p));

    while (--p >= numbers)
    {   char *du_p = strdup(*p);
//	printf("table[\"%s\"] = %4g\n", du_p, a.find(du_p));
	float found = a.find(du_p);
	assert(found == (float) atof(*p));
	free(du_p);
    }

    for (p = numbers; p[0] && p[1]; p += 2)
    {   char *du_p = strdup(*p);
	a.remove(du_p);
	free(du_p);
    }

    while (--p >= numbers)
    {   char *du_p = strdup(*p);
//	printf("table[\"%s\"] = %4g\n", du_p, a.find(du_p));
	float found = a.find(du_p);
	assert((p - numbers) % 2 || !found);
	assert(!((p - numbers) % 2) || found == (float) atof(*p));
	free(du_p);
    }

    return 0;
}

#endif /* UNIT_TEST_StringTable */

#endif /* !StringTable_C_included */
