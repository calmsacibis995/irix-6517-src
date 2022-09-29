/*
 * purge.c - invalidate all entries in the tlb
 *
 * $Source: /proj/irix6.5.7m/isms/stand/arcs/ide/IP30/tlb/RCS/purge.c,v $
 * $Revision: 1.4 $
 * $Date: 1993/11/02 23:47:08 $
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
