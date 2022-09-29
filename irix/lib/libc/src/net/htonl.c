#ifdef __STDC__
	#pragma weak htonl = _htonl
#endif
#if !defined(NOWEAK)
#define	htonl	_htonl
#endif
#include "synonyms.h"

unsigned long
htonl(unsigned long x)
{
	return (x);
}
