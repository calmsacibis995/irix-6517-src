#ifndef operators_included
#define operators_included

#include "bool.H"

template <class L, class R>
inline L
operator + (const L& left, const R& right)
{
    L result = left;
    result += right;
    return result;
}

template <class L, class R>
inline L
operator - (const L& left, const R& right)
{
    L result = left;
    result -= right;
    return result;
}

template <class L, class R>
inline bool
operator != (const L& left, const R& right)
{
    return !(left == right);
}

#endif /* !operators_included */
