/*
 * purge.c - invalidate all entries in the tlb
 *
 * $Source: /proj/irix6.5.7m/isms/stand/arcs/ide/IP26/tlb/RCS/purge.c,v $
 * $Revision: 1.2 $
 * $Date: 1994/05/14 01:44:52 $
 */
#include <sys/types.h>
#include <sys/sbd.h>
#include "tlb.h"
#include "libsk.h"

void invaltlb_in_set(int,int);

void
tlbpurge(void)
{
	int i, j;

#if TFP

	for (j = 0; j < NTLBSETS; j++)  {

		for (i = 0; i < NTLBENTRIES; i++) {
			invaltlb_in_set(i,j);
	}

} 

#else






	for (i = 0; i < NTLBENTRIES; i++)
		invaltlb(i);

#endif
}


