#ifdef __STDC__
	#pragma weak ntohl = _ntohl
#endif
#if !defined(NOWEAK)
#define	ntohl	_ntohl
#endif
#include "synonyms.h"

unsigned long
ntohl(unsigned long x)
{
	return (x);
}
