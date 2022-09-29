#include "Enumerable.H"

#include <assert.h>
#include <stddef.h>

//  All Enumerables in a set are on a doubly-linked list.  _set._first
//  points to its head and _set._last points to its tail.  this->_next
//  and this->_prev are the forward and backward pointers.
//
//  _set._first->_prev is always NULL.  So is _set._last->_next.

Enumerable::Enumerable(Set& set)
: _set(set)
{
    if (_set._n++)
    {   _prev = set._last;
	_next = NULL;
	_prev->_next = this;
	_set._last = this;
    }
    else
    {   _set._first = _set._last = this;
	_next = _prev = NULL;
    }
}

Enumerable::~Enumerable()
{
    _set._n--;
    if (_next)
	_next->_prev = _prev;
    else
    {   assert(_set._last = this);
	_set._last = _prev;
    }
    if (_prev)
	_prev->_next = _next;
    else
    {   assert(_set._first == this);
	_set._first = _next;
    }
}

unsigned int
Enumerable::index() const
{
    Enumerable *p = _set.first();
    for (unsigned int i = 0; p != this; i++, p = p->next())
	assert(p);
    return i;
}

Enumerable *
Enumerable::Set::nth(unsigned int index)
{
    Enumerable *p = first();
    for (unsigned int i = 0; p && i < index; i++, p = p->next())
	continue;
    return p;
}

#ifndef NDEBUG

//  I'm assuming that all Sets are stored in .bss.  If that's not
//  true, Set needs constructor and destructor.

Enumerable::Set::Set()
{
    assert(_n == 0);
    assert(_first == NULL);
    assert(_last == NULL);
}

Enumerable::Set::~Set()
{
    assert(_n == 0);
    assert(_first == NULL);
    assert(_last == NULL);
}

#endif /* !NDEBUG */
