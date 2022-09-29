#ifndef timeval_included
#define timeval_included

#include <sys/time.h>

//////////////////////////////////////////////////////////////////////////////
//  Define a few arithmetic ops on timevals.  Makes code much more
//  readable.
//
//  operators += and -= are defined in timeval.C.  The rest are
//  defined as inline functions.

timeval& operator += (timeval& left, const timeval& right);
timeval& operator -= (timeval& left, const timeval& right);

inline timeval
operator + (const timeval& left, const timeval& right)
{
    timeval result = left;
    result += right;
    return result;
}

inline timeval
operator - (const timeval& left, const timeval& right)
{
    timeval result = left;
    result -= right;
    return result;
}

inline int
operator < (const timeval& left, const timeval& right)
{
    return timercmp(&left, &right, < );
}

inline int
operator > (const timeval& left, const timeval& right)
{
    return timercmp(&left, &right, > );
}

inline int
operator >= (const timeval& left, const timeval& right)
{
    return !(left < right);
}

inline int
operator <= (const timeval& left, const timeval& right)
{
    return !(left > right);
}

inline int
operator == (const timeval& left, const timeval& right)
{
    return timercmp(&left, &right, == );
}

inline int
operator != (const timeval& left, const timeval& right)
{
    return timercmp(&left, &right, != );
}

#endif /* !timeval_included */
