#ifdef __STDC__
	#pragma weak htons = _htons
#endif
#if !defined(NOWEAK)
#define	htons	_htons
#endif
#include "synonyms.h"

unsigned short
htons(unsigned short x)
{
	return (x);
}
