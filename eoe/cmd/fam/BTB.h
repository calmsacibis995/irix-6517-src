#ifndef BTB_included
#define BTB_included

#include "Boolean.h"

//  This is an in-core B-Tree implementation.
//
//  The public interface is fairly sparse: find, insert and remove
//  are the provided functions.  insert() and remove() return true
//  on success, false on failure.  find() returns 0 on failure.
//
//  size() returns the number of key/value pairs in the tree.
//
//  sizeofnode() returns the size of a BTB node, in bytes.
//
//  The Key class must have the operators == and < defined.
//  The Value class must have a zero element.
//  The fanout specifies the maximum number of children a node
//  of the BTB may have.

class BTB {

    struct Node;

public:

    typedef unsigned int Key, Value;
    enum { fanout = 32 };

    BTB();
    ~BTB();

    Value find(const Key&) const;
    Boolean insert(const Key&, const Value&); // true if successful
    Boolean remove(const Key&);		      // true if successful

    Key first() const;
    Key next(const Key&) const;

    unsigned size() const		{ return npairs; }

    static unsigned sizeofnode()	{ return sizeof (Node); }

#ifdef UNIT_TEST_BTB
    void print(Node * = 0, int = 1) const;
#endif /* !UNIT_TEST_BTtree */

private:

    enum Status { OK, NO, OVER, UNDER };

    struct Closure {

	Closure(Status s)		    : status(s) { }
	Closure(Key k, Value v, Node *l)    : status(OVER), key(k), value(v),
					      link(l) { }
	Closure(Status s, const Closure& i) : status(s), key(i.key),
					      value(i.value), link(i.link) { }
	Closure(Status s, const Key& k)	    : status(s), key(k) { }
	operator Status ()		    { return status; }

	Status status;
	Key    key;
	Value  value;
	Node  *link;

    };

    struct Node {

	Node(Node *, const Closure&);
	Node(Node *, unsigned index);
	~Node();

	unsigned find(const Key&) const;
	Closure next(const Key&) const;
	Boolean insert(unsigned, const Closure&);
	Closure remove(unsigned);
	void join(const Closure&, Node *);

	unsigned n;
	Key   key  [fanout];
	Node *link [fanout + 1];
	Value value[fanout];

    private:

	Node(const Node&);		// Do not copy
	operator = (const Node&);	// or assign.

    };

    // Instance Variables

    Node *root;
    unsigned npairs;

    Closure insert(Node *, const Key&, const Value&);
    Status remove(Node *, const Key&);
    Status underflow(Node *, unsigned);
    Closure remove_rightmost(Node *);

    BTB(const BTB&);			// Do not copy
    operator = (const BTB&);		//  or assign.

friend class BTB::Closure;
friend class BTB::Node;

};

#endif /* !BTB_included */
