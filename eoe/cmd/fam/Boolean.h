#ifndef Boolean_included
#define Boolean_included

#include <assert.h>

//  The good old Boolean.  If NDEBUG is defined, it's simply an int.
//  If NDEBUG is undefined, then Boolean is a class that has error
//  checking so that an uninitialized instance can't be used.

#ifdef NDEBUG

typedef int Boolean;

#else

class Boolean {

public:

    Boolean()				: b(2) { }
    Boolean(int i)			: b(i) { assert(i == 0 || i == 1); }
    operator int () const		{ assert(b == 0 || b == 1); return b; }

private:

    int b;

};

#endif /* NDEBUG */

const Boolean true(1), false(0);

#endif /* !Boolean_included */
