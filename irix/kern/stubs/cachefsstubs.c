#include "sys/types.h"
#include "sys/errno.h"

int
cachefs_sys()
{
	return(ENOSYS);
}

void
cachefs_clearstat()
{
	return;
}

/* ARGSUSED */
caddr_t
cachefs_getstat(int procnum)
{
	return(0);
}


void cachefs_icrash(void) { }
