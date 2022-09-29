/*
 * purge.c - invalidate all entries in the tlb
 *
 * $Source: /proj/irix6.5.7m/isms/stand/arcs/ide/IP32/tlb/RCS/purge.c,v $
 * $Revision: 1.2 $
 * $Date: 1997/05/15 16:09:24 $
 */
#include <sys/types.h>
#include <sys/sbd.h>
#include "tlb.h"
#include "libsk.h"

void
tlbpurge(void)
{
	int i;

	for (i = 0; i < NTLBENTRIES; i++)
		invaltlb(i);
}
