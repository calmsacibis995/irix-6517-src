/*
 * BSD compatibility routine
 */
#ident "$Id: getdtabsz.c,v 1.5 1994/10/18 04:16:43 jwag Exp $"

#ifdef __STDC__
	#pragma weak getdtablesize = _getdtablesize
#endif
#include "synonyms.h"

#include <ulimit.h>

int
getdtablesize(void)
{
	return (int)ulimit(4,0);	/* present but undocumended in SVR3 */
}
