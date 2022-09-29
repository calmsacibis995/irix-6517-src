#ident "$Revision: 1.4 $"

#ifdef __STDC__
	#pragma weak blockproc = _blockproc
	#pragma weak unblockproc = _unblockproc
	#pragma weak setblockproccnt = _setblockproccnt
	#pragma weak blockprocall = _blockprocall
	#pragma weak unblockprocall = _unblockprocall
	#pragma weak setblockproccntall = _setblockproccntall
#endif
#include "synonyms.h"
#include <sys/types.h>	/* for pid_t/unsigned int */

/*
 * interface to process blocking routines
 */

extern int procblk(unsigned int, pid_t, int);

int
blockproc(pid_t pid)
{
	return(procblk(0, pid, 0));
}

int
unblockproc(pid_t pid)
{
	return(procblk(1, pid, 0));
}

int
setblockproccnt(pid_t pid, int count)
{
	return(procblk(2, pid, count));
}

int
blockprocall(pid_t pid)
{
	return(procblk(3, pid, 0));
}

int
unblockprocall(pid_t pid)
{
	return(procblk(4, pid, 0));
}

int
setblockproccntall(pid_t pid, int count)
{
	return(procblk(5, pid, count));
}
