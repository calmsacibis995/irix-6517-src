#include "timeval.h"

timeval&
operator += (timeval& left, const timeval& right)
{
    left.tv_sec += right.tv_sec;
    left.tv_usec += right.tv_usec;
    while (left.tv_usec >= 1000000)
    {   left.tv_usec -= 1000000;
	left.tv_sec += 1;
    }
    return left;
}

timeval&
operator -= (timeval& left, const timeval& right)
{
    left.tv_sec -= right.tv_sec;
    left.tv_usec -= right.tv_usec;
    while (left.tv_usec < 0)
    {   left.tv_usec += 1000000;
	left.tv_sec -= 1;
    }
    return left;
}
