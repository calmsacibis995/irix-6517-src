#ifndef SmallTable_included
#define SmallTable_included

template <class T> class StringTable;

//  A SmallTable holds key-value mappings.  Its implementation is
//  cheap and dirty, so it should not be used for tables with lots of
//  entries.  Lookups are faster than insertions/removals.
//
//  Operations:
//		insert()	puts a new entry in table.
//		remove()	removes an entry.
//		find()		returns the value for a key.
//		size()		returns number of entries.


template <class Tkey, class Tvalue> class SmallTable {

public:

    SmallTable()			: n(0), nalloc(0), table(0) { }
    virtual ~SmallTable();

    void insert(const Tkey&, const Tvalue&);
    void remove(const Tkey&);
    inline Tvalue find(const Tkey& k) const;
    Tkey first() const			{ return n ? table[0].key : 0; }
    unsigned size() const		{ return n; }

private:

    struct Closure {
	const enum Presence { PRESENT, ABSENT } presence;
	const unsigned index;
	Closure(Presence p, unsigned i)	: presence(p), index(i) { }
    };

    struct Pair {
	Tkey key;
	Tvalue value;
    };

    unsigned n, nalloc;
    Pair *table;

    virtual Closure position(const Tkey&) const;

    SmallTable(const SmallTable&);	// Do not copy
    operator = (const SmallTable&);	//  or assign.

};

template <class Tkey, class Tvalue>
inline Tvalue
SmallTable<Tkey, Tvalue>::find(const Tkey& k) const
{
    Closure c = position(k);
    return c.presence == Closure::PRESENT ? table[c.index].value : 0;
}

#endif /* !SmallTable_included */
