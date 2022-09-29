#ifndef bool_included
#define bool_included

#if !defined(_COMPILER_VERSION) || _COMPILER_VERSION < 700
// bool is builtin if 7.00 or later

//  This whole file goes away with the arrival of ANSI C++.

#ifdef NDEBUG

typedef int bool;

const bool true = 1, false = 0;

#else /* NDEBUG */

class bool {

public:

    bool()				: _b(2) { }
    bool(const bool& that)		: _b(that._b) { }
    bool(int);
    operator int () const;

private:

    int _b;

};

extern const bool true, false;

#endif /* NDEBUG */

#endif /* new compilers */

#endif /* !bool_included */
