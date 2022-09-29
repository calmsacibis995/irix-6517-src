#ifdef __STDC__
	#pragma weak ntohs = _ntohs
#endif
#if !defined(NOWEAK)
#define	ntohs	_ntohs
#endif
#include "synonyms.h"

unsigned short
ntohs(unsigned short x)
{
	return (x);
}
