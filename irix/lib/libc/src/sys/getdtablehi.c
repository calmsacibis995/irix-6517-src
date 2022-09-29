#ident "$Id: getdtablehi.c,v 1.3 1994/10/03 04:31:40 jwag Exp $"

#ifdef __STDC__
	#pragma weak getdtablehi = _getdtablehi
#endif
#include "synonyms.h"
#include "sys/syssgi.h"
#include "unistd.h"

int
getdtablehi(void)
{
	int fds;

	if ((fds = (int)syssgi(SGI_FDHI)) <= 0)
		fds = getdtablesize();
	return fds;
}
