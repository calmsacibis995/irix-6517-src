#ifndef BTree_included
#define BTree_included

#include "BTB.h"

//  This is an in-core B-Tree implementation.
//
//  The public interface is fairly sparse: find, insert and remove
//  are the provided functions.  insert() and remove() return true
//  on success, false on failure.  find() returns 0 on failure.
//
//  size() returns the number of key/value pairs in the tree.
//
//  sizeofnode() returns the size of a BTree node, in bytes.
//
//  The Key class must have the operators == and < defined.
//  The Value class must have a zero element.

template <class K, class V> class BTree : public BTB {

public:

#ifndef NDEBUG
    BTree();				// Assertions only.
#endif

    V find(const K& k) const		{ return (V) BTB::find(Key(k)); }
    Boolean insert(const K& k, const V& v)
				      { return BTB::insert(Key(k), Value(v)); }
    Boolean remove(const K& k)		{ return BTB::remove(Key(k)); }

    K first() const			{ return (K) BTB::first(); }
    K next(const K& k) const		{ return (K) BTB::next(Key(k)); }

};

#endif /* !BTree_included */
