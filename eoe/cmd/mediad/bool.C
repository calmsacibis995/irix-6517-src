#if !defined(_COMPILER_VERSION) || _COMPILER_VERSION < 700
// bool is builtin if 7.00 or later

#include "bool.H"

#include <assert.h>

#ifndef NDEBUG

const bool true = 1, false = 0;

bool::bool(int i)
: _b(i)
{
    assert(i == 0 || i == 1);
}

bool::operator int () const
{
    assert(_b == 0 || _b == 1);
    return _b;
}

#endif /* !NDEBUG */
#endif
