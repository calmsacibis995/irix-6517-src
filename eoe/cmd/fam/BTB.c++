#ifndef BTB_C_included
#define BTB_C_included

#include "BTB.h"

#include <assert.h>
#include <stddef.h>

//  TODO: cache
//  TODO: comments

///////////////////////////////////////////////////////////////////////////////
//  Node code

//  Construct a Node from a Node and a Closure.  New node
//  has a single key-value pair, its left child
//  is the other node, and its right child is the link
//  field of the closure.

BTB::Node::Node(Node *p, const Closure& closure)
{
    n = 1;
    link[0] = p;
    key[0] = closure.key;
    value[0] = closure.value;
    link[1] = closure.link;
}

//  Construct a Node from a Node and an index.  Steals
//  the elements index..n of other Node for itself.
//  When this constructor is done, both this->link[0] and
//  that->link[that->n] point to the same place.  The caller
//  must resolve that.

BTB::Node::Node(Node *that, unsigned index)
{
    n = that->n - index;
    for (int i = 0; i < n; i++)
    {   key[i] = that->key[index + i];
	value[i] = that->value[index + i];
	link[i] = that->link[index + i];
    }
    link[n] = that->link[index + n];
    that->n = index;
}

//  Node destructor.  Recursively deletes subnodes.

BTB::Node::~Node()
{
    for (int i = 0; i <= n; i++)
	delete link[i];
}

//  Node::find() uses binary search to find the nearest key.
//  return index of key that matches or of next key to right.
//
//  E.g., if find() returns 3 then either key[3] matches or
//  key passed in is between key[3] and key[4].

unsigned
BTB::Node::find(const Key& k) const
{
    unsigned l = 0, r = n;
    while (l < r)
    {   int m = (l + r) / 2;
	if (k == key[m])
	    return m;
	else if (k < key[m])
	    r = m;
	else // (k > key[m])
	    l = m + 1;
    }
    assert(l == n || k < key[l]);
    return l;
}

//  Node::insert() inserts key/value into j'th position in node.
//  Inserts closure's link to the right.  Returns true if successful,
//  false if node is already full.

Boolean
BTB::Node::insert(unsigned j, const Closure& closure)
{
    if (n < fanout)
    {   for (int i = n; i > j; --i)
	{   key[i] = key[i - 1];
	    value[i] = value[i - 1];
	    link[i + 1] = link[i];
	}
	key[j] = closure.key; value[j] = closure.value;
	link[j + 1] = closure.link;
	n++;
	assert(j == 0     || key[j - 1] < key[j]);
	assert(j == n - 1 || key[j] < key[j + 1]);
	return true;
    }
    else
	return false;
}

//  Node::remove extracts the j'th key/value and the link
//  to the right and returns them.

BTB::Closure
BTB::Node::remove(unsigned j)
{
    Key k = key[j];
    Value v = value[j];
    Node *l = link[j + 1];
    for (unsigned i = j + 1; i < n; i++)
    {   key[i - 1] = key[i];
	value[i - 1] = value[i];
	link[i] = link[i + 1];
    }
    --n;
    return Closure(k, v, l);
}

// Node::join() copies key/value from closure and moves all
// key/value/links from other Node into this node.
// Other Node is modified to contain no keys, no values, no links.

void
BTB::Node::join(const Closure& it, Node *that)
{
    assert(that);
    assert(n + that->n <= fanout - 1);
    key[n] = it.key;
    value[n] = it.value;
    for (int i = 0, j = n + 1; i < that->n; i++, j++)
    {   key[j] = that->key[i];
	value[j] = that->value[i];
	link[j] = that->link[i];
    }
    n += that->n + 1;
    link[n] = that->link[that->n];
    that->n = 0;
    that->link[0] = NULL;
}

///////////////////////////////////////////////////////////////////////////////

//  BTB constructor -- check that fanout is even.

BTB::BTB()
    : root(NULL), npairs(0)
{
    assert(!(fanout % 2));
}

//  BTB destructor -- delete all Nodes.

BTB::~BTB()
{
    delete root;
}

//  BTB::find() -- search BTB for Key, return matching value.

BTB::Value
BTB::find(const Key& key) const
{
    for (Node *p = root; p; )
    {   int i = p->find(key);
	if (i < p->n && key == p->key[i])
	    return p->value[i];
	else
	    p = p->link[i];
    }
    return Value(0);
}

BTB::Key
BTB::first() const
{
    if (!root)
	return 0;
    assert(root->n);

    Node *p, *q;
    for (p = root; q = p->link[0]; p = q)
	continue;
    return p->key[0];
}

BTB::Key
BTB::next(const Key& pred) const
{
    if (!root)
	return 0;

    assert(root->n);

    Closure it = root->next(pred);
    switch (Status(it))
    {
    case OK:
	return it.key;

    case OVER:
	return 0;			// All done.

    default:
	assert(0);
	return 0;
    }
}

BTB::Closure
BTB::Node::next(const Key& pred) const
{
    if (!this)
	return OVER;

    unsigned i = find(pred);
    assert(i <= n);
    if (i < n && key[i] == pred)
	i++;
    Closure it = link[i]->next(pred);
    if (Status(it) != OVER)
	return it;
    if (i == n)
	return OVER;
    else
	return Closure(OK, key[i]);
}

//  BTB::insert() -- insert a new key/value pair
//  into the tree.  Fails and returns false if key is already
//  in tree.
//
//  This code is the only place that makes the tree taller.

Boolean
BTB::insert(const Key& key, const Value& value)
{
    Closure it = insert(root, key, value);
    switch (Status(it))
    {
    case OK:
	npairs++;
	return true;

    case NO:
	return false;

    case OVER:
	root = new Node(root, it);
	npairs++;
	return true;

    default:
	assert(0);
	return false;
    }
}

//  BTB::insert(Node *, ...) -- helper function for
//  BTB::insert(Key&, ...)  Recurses to the appropriate leaf, splits
//  nodes as necessary on the way back.

BTB::Closure
BTB::insert(Node *p, const Key& key, const Value& value)
{
    if (!p)
	return Closure(key, value, NULL);
    int i = p->find(key);
    if (i < p->n && key == p->key[i])
	return NO;			// key already exists.

    Closure it = insert(p->link[i], key, value);

    if (Status(it) == OVER)
    {   if (p->insert(i, it))
	    return Closure(OK);
	else
	{   if (i > fanout / 2)
	    {   Node *np = new Node(p, fanout / 2 + 1);
		np->insert(i - fanout / 2 - 1, it);
		assert(p->n > fanout / 2);
		it = p->remove(fanout / 2);
		return Closure(it.key, it.value, np);
	    }
	    else if (i < fanout / 2)
	    {
		Node *np = new Node(p, fanout / 2);
		p->insert(i, it);
		assert(p->n > fanout / 2);
		it = p->remove(fanout / 2);
		return Closure(it.key, it.value, np);
	    }
	    else // (i == fanout / 2)
	    {
		Node *np = new Node(p, fanout / 2);
		np->link[0] = it.link;
		return Closure(it.key, it.value, np);
	    }
	}
    }
    
    return it;
}

//  BTB::remove() -- remove a key/value from the tree.
//  This function is the only one that makes the tree shorter.

Boolean
BTB::remove(const Key& key)
{
    Status s = remove(root, key);
    switch (s)
    {
    case OK:
	assert(npairs);
	--npairs;
	assert(!root || root->n);
	return true;

    case NO:
	assert(!root || root->n);
	return false;

    case UNDER:
	if (root->n == 0)
	{   Node *nr = root->link[0];
	    root->link[0] = NULL;	// don't delete subtree
	    delete root;
	    root = nr;
	}
	assert(npairs);
	--npairs;
	assert(!root || root->n);
	return true;

    default:
	assert(0);
	assert(!root || root->n);
	return false;
    }
}

//  BTB::underflow -- helper function to BTB::remove() When a node
//  has too few elements (less than fanout / 2), this function is
//  invoked.  It tries to join i'th child of node p with one of its
//  adjacent siblings, then tries to move entries from an adjacent
//  sibling to child node.
//
//  Returns UNDER if node p is too small afterward, OK otherwise.

BTB::Status
BTB::underflow(Node *p, unsigned i)
{
    assert(p);
    assert(i <= p->n);
    Node *cp = p->link[i];
    assert(cp);
    
    Node *rp = i < p->n ? p->link[i + 1] : NULL;
    Node *lp = i > 0    ? p->link[i - 1] : NULL;
    assert(!rp || rp->n >= fanout / 2);
    assert(!lp || lp->n >= fanout / 2);

    if (rp && rp->n == fanout / 2)
    {
	// join cp w/ rp;
	cp->join(p->remove(i), rp);
	delete rp;
    }
    else if (lp && lp->n == fanout / 2)
    {
	// join lp w/ cp;
	lp->join(p->remove(i - 1), cp);
	delete cp;
    }
    else if (lp)
    {
	// shuffle from lp to cp;
	Closure li = lp->remove(lp->n - 1);
	cp->insert(0, Closure(p->key[i - 1], p->value[i - 1], cp->link[0]));
	cp->link[0] = li.link;
	p->key[i - 1] = li.key;
	p->value[i - 1] = li.value;
	return OK;
    }
    else if (rp)
    {
	// shuffle from rp to cp;
	Closure ri = rp->remove(0);
	cp->insert(cp->n, Closure(p->key[i], p->value[i], rp->link[0]));
	p->key[i] = ri.key;
	p->value[i] = ri.value;
	rp->link[0] = ri.link;
	return OK;
    }
    
    return p->n >= fanout / 2 ? OK : UNDER;
}

//  BTB::remove_rightmost() -- helper to BTB::remove().
//
//  Removes rightmost leaf key/value from subtree and returns them.
//  If Nodes become too small in the process, invokes Node::underflow()
//  to fix them up.  Return status is either OK or UNDER, indicating
//  root of subtree is too small.


BTB::Closure
BTB::remove_rightmost(Node *p)
{
    int i = p->n;
    Node *cp = p->link[i];
    if (cp)
    {   Closure it = remove_rightmost(cp);
	if (Status(it) == UNDER)
	    return Closure(underflow(p, i), it);
	else
	    return it;
    }
    else
    {   Closure it = p->remove(p->n - 1);
	if (p->n >= fanout / 2)
	    return Closure(OK, it);
	else
	    return Closure(UNDER, it);
    }	    
}

//  BTB::remove(Node *, ...) -- helper to BTB::remove(const Key&).
//
//  Recurses into tree to find key/value to delete.  When it finds
//  them, it pulls the rightmost leaf key/value off the branch to
//  the left and replaces the removed k/v with them.  Watches for
//  Node underflows from BTB::remove_rightmost() and propagates them
//  back up.

BTB::Status
BTB::remove(Node *p, const Key& key)
{
    if (!p)
	return NO;			// key not found
    int i = p->find(key);
    if (i < p->n && key == p->key[i])
    {
	// Delete this one.

	Closure it = p->remove(i);
	if (p->link[i])
	{   Closure rm = remove_rightmost(p->link[i]);
	    assert(!rm.link);
	    p->insert(i, Closure(rm.key, rm.value, it.link));
	    if (Status(rm) == UNDER)
		return underflow(p, i);
	}
	return p->n >= fanout / 2 ? OK : UNDER;
    }
    else
    {   Node *cp = p->link[i];
	Status s = remove(cp, key);
	if (s == UNDER)
	    return underflow(p, i);
	else
	    return s;
    }
}

#ifdef UNIT_TEST_BTB

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//  Handy printing function for testing.

void
BTB::print(Node *p, int level) const
{
    if (!p)
    {
	p = root;
	printf("------------------\n");
    }
    if (!p)
	return;
    assert(p->n <= fanout);
    assert(p == root || p->n >= fanout / 2);

    if (p->link[0])
    {
	for (int i = 0; i < p->n; i++)
	{   assert(p->link[i + 1]);
	    print(p->link[i], level + 1);
	    printf("%*d\n", 6 * level, p->key[i]);
	}
	print(p->link[p->n], level + 1);
    }
    else
    {   for (int i = 0; i < p->n; i++)
	{   printf("%*d", 6 * level, p->key[i]);
	    level = 1;
	    assert(!p->link[i + 1]);
	}
	printf("\n");
    }
    if (p == root)
	fflush(stdout);
}

typedef BTB PalmTree;

class dummy {
    PalmTree d;
};

void function()
{
    const int N = 17;
    int i, j, k;

    printf("sizeof node = %d\n", PalmTree::sizeofnode());

    for (k = 0; k < 100; k++)
    {
	// Test tree insertion.

	for (i = 1; i < N; i++)
	{   PalmTree pt;
	    for (j = 0; j < N; j++)
	    {   int key = i * (j + 1) % N;
		pt.insert(key, 1.01 * key);
	    }
	    for (j = 0; j < N; j++)
		assert((int) pt.find(j) == j);
	    int key = pt.first();
	    for (j = 0; j < N; j++)
	    {   assert(key == j);
		key = pt.next(key);
	    }
	}

	// Test tree removal.

	for (i = 1; i < N; i++)
	{   PalmTree pt;
	    for (j = 0; j < N; j++)
		pt.insert(j, 1.01 * j);
	    for (j = 0; j < N; j++)
	    {   int key = i * (j + 1) % N;
		Boolean b = pt.remove(key);
		assert(b);
	    }
	}
    }
}

void stress(int niter)
{
    enum { NKEYS = 10000, MAXKEY = 2 * NKEYS };
    enum { NVALUES = 2 * NKEYS, MAXVALUE = NVALUES / 3 };

    int keys[NKEYS];
    float values[NVALUES];
    float table[NKEYS];
    int i, nt;

    // Generate a heap (as in heapsort) of random keys.

    for (i = 0; i < NKEYS; i++)
    {   int c, p, key = random() % MAXKEY;
	for (c = i; (p = c - 1 >> 1) >= 0; c = p)
	{   if (keys[p] >= key)
		break;
	    keys[c] = keys[p];
	}
	keys[c] = key;
    }

    // Sort the heap.

    for (i = NKEYS; --i >= 0; )
    {   int c, p, t = keys[0];
	for (p = 0; (c = 2 * p + 1) < i; p = c)
	{   int s = c + 1;
	    if (s < i && keys[s] > keys[c])
		c = s;
	    keys[p] = keys[c];
	}
	keys[i] = t;
    }

    // Check that it's sorted.

    for (i = 1; i < NKEYS; i++)
	assert(keys[i - 1] <= keys[i]);

    // Fix up duplicates.

    for (i = 1; i < NKEYS; i++)
	if (keys[i] <= keys[i - 1])
	    keys[i] = keys[i - 1] + 1;

    // Generate some random values.

    for (i = 0; i < NVALUES; i++)
	values[i] = random() % MAXVALUE + 1;
    nt = 0;
    printf("init\n");

    // Make a BTB and start exercising it.

    PalmTree pt;
    for (i = 0; !niter || i < niter; i++)
    {   int op = random() % 4;
	int ki, vi;
	switch (op)
	{
	case 0:				// find
	    ki = random() % NKEYS;
	    float frv = pt.find(keys[ki]);
	    assert(frv == table[ki]);
	    break;

	case 1:				// insert
	    ki = random() % NKEYS;
	    vi = random() % NVALUES;
	    Boolean irv = pt.insert(keys[ki], values[vi]);
	    if (table[ki])
		assert(irv == false);
	    else
	    {   assert(irv == true);
		table[ki] = values[vi];
		nt++;
	    }
	    break;

	case 2:				// remove
	    ki = random() % NKEYS;
	    Boolean rrv = pt.remove(keys[ki]);
	    if (table[ki])
	    {   assert(rrv);
		table[ki] = 0;
		nt--;
	    }
	    else
		assert(!rrv);
	    break;

	case 3:				// size
	    int srv = pt.size();
	    assert(srv == nt);
	    break;

	default:
	    assert(0);
	}
    }
}

main(int argc, char **argv)
{
    if (argc > 1)
    {   if (!strcmp(argv[1], "-s"))
	    stress(argv[2] ? atoi(argv[2]) : 0);
    }
    else
	function();
    return 0;
}

#endif /* UNIT_TEST_BTB */

#endif /* !BTB_C_included */
