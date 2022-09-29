#ifndef Bag_C_included
#define Bag_C_included

#include "Bag.h"

#include <assert.h>

template <class T>
Bag<T>::~Bag()
{
    while (p)
    {   Chunk *first = p;
	p = p->next;
	delete first;
    }
}

template <class T>
Bag<T>&
Bag<T>::operator = (const Bag& that)
{
    if (this != &that)
    {   ~Bag();
	for (Chunk *tp = that.p; tp; tp = tp->next)
	    for (int i = 0; i < tp->n; i++)
		insert(tp->them[i]);
    }
    return *this;
}

template <class T>
void
Bag<T>::insert(const T& goodie)
{
    if (!p || p->n == ENUF)
	p = new Chunk(p);
    assert(p);
    assert(p->n < ENUF);
    p->them[p->n++] = goodie;
}

template <class T>
T
Bag<T>::take()
{
    if (!p)
	return 0;
    assert(p->n > 0);
    T it = p->them[--p->n];
    if (!p->n)
    {   Chunk *next = p->next;
	delete p;
	p = next;
    }
    return it;
}

#ifdef UNIT_TEST_Bag

int main()
{
    const int N = 22, K = 1000;
    Bag<float> sack;
    int i, A[N];
    assert(sack.empty());
    for (i = 0; i < N; i++)
    {	sack.insert(i + K);
	assert(!sack.empty());
	A[i] = 0;
    }
    for (i = 0; i < N; i++)
    {   assert(!sack.empty());
	float f = sack.take();
	assert(K <= f && f < N + K);
	assert(A[(int) (f - K)] == 0);
	A[(int) (f - K)] = 1;
    }
    assert(sack.empty());
    float h = sack.take();
    assert(h == 0);
    assert(sack.empty());

    return 0;
}

#endif /* UNIT_TEST_Bag */

#endif /* !Bag_C_included */
