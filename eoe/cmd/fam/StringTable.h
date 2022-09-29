#ifndef StringTable_included
#define StringTable_included

#include "Boolean.h"

//  A StringTable maps C strings onto values.  It is a cheap and dirty
//  implementation, suitable only for small tables that are
//  infrequently searched.

template <class Tv> class StringTable {

    typedef const char *Tk;

public:

    StringTable()			: n(0), nalloc(0), table(0)
					{ }
    virtual ~StringTable();
    StringTable& operator = (const StringTable&);

    inline Tv find(const Tk) const;
    unsigned size() const		{ return n; }
    Tk key(unsigned i)			{ return i < n ? table[i].key : 0; }
    Tv value(unsigned i)		{ return i < n ? table[i].value : 0; }

    void insert(const Tk, const Tv&);
    void remove(const Tk);

private:

    struct Pair {
	char *key;
	Tv value;
    };

    unsigned n, nalloc;
    Pair *table;

    virtual signed int position(const Tk) const;

    StringTable(const StringTable&);	// Do not copy.

};

template <class Tv>
inline Tv
StringTable<Tv>::find(const Tk k) const
{
    signed int index = position(k);
    return index >= 0 ? table[index].value : 0;
}

#endif /* !StringTable_included */
