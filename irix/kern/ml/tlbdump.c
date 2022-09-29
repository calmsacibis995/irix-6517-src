/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.14 $"


#include <sys/types.h>
#include <sys/debug.h>
#include <sys/dump.h>
#include <sys/sbd.h>
#include <sys/tlbdump.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/pda.h>


tlbinfo_t       *tlbdumptbl = (tlbinfo_t *)0;

void
dumptlb (int id)
{
        int     i;
#if     R4000 || TFP
	int	 j;
#endif
        tlbinfo_t *tinfo;

        if (tlbdumptbl == (tlbinfo_t *)0) /* Array not allocated ?? */
                return;
        tinfo = &tlbdumptbl[id];

        for (i = 0; i < NTLBENTRIES; i++) {
#if     R4000 || R10000
                tlbgetent(i, (void *)&tinfo->tlbents[i].tlbhi,
                             (void *)&tinfo->tlbents[i].tlblo.pgi,
                             (void *)&tinfo->tlbents[i].tlblo1.pgi);
#elif TFP
        	for (j = 0; j < NTLBSETS; j++) {
			tlbgetent(i, j,
			     (void *)&tinfo->tlbents[i][j].tlbhi,
			     (void *)&tinfo->tlbents[i][j].tlblo.pgi);
		}
#elif BEAST
		printf ("FIXME: dumptlb on BEAST\n");

#else
        ?? unknown CPU Type to dump TLB ??
#endif
        }

        tinfo->tlbentry_valid = (id << 8) | 1;

	/*
	 * Check for duplicate entries
	 */
#if     R4000
	for (i = 0; i < NTLBENTRIES - 1; i++) {
	    if ((tinfo->tlbents[i].tlbhi & TLBHI_VPNMASK) ==
		PTR_EXT(PHYS_TO_K0(0) & ~TLBHI_VPNZEROFILL))
		continue;

	    for (j = i+1; j < NTLBENTRIES; j++) {
		if ((tinfo->tlbents[j].tlbhi & TLBHI_VPNMASK) ==
		    PTR_EXT(PHYS_TO_K0(0) & ~TLBHI_VPNZEROFILL))
			continue;

		if ((tinfo->tlbents[i].tlbhi & (TLBHI_VPNMASK|TLBHI_PIDMASK)) ==
		    (tinfo->tlbents[j].tlbhi & (TLBHI_VPNMASK|TLBHI_PIDMASK))) {
			printf("NOTICE - cpu %d has duplicate tlb entries "
			       "(%d, %d).\n", id, i, j);
			return;
			}
		}
	}
#endif
#if	TFP
	for (i = 0; i < NTLBENTRIES; i++) {
		__psunsigned_t vaddrs[NTLBSETS], pids[NTLBSETS], vaddr;
		k_machreg_t tlbhi;
		long xormask;

		/*
		 * Need to compute bits 18:12 of the Virtual Address
		 * since they are implied by the index number XORed
		 * with the PID.  Just "or" them into tlbhi contents
		 * since the bits should be zero.
		 */
		for (j = 0; j < NTLBSETS; j++) {
			tlbhi = tinfo->tlbents[i][j].tlbhi;
			vaddr = tlbhi & TLBHI_VPNMASK;
			pids[j] = (tlbhi & TLBHI_PIDMASK) >> TLBHI_PIDSHIFT;
			/*
			 * Need to 'xor' low order virtual address with the
			 * PID unless the address is in KV1 space
			 * (kernel global).
			 */
			xormask = ((tlbhi & KV1BASE) != KV1BASE)
						? (pids[j] & 0x7f)
						: 0;
			vaddr |= ((i ^ xormask) << PNUMSHFT);
			vaddrs[j] = vaddr;
		}
#if NTLBSETS != 3
BOMB!
#endif
		if ((pids[0] == pids[1] && vaddrs[0] == vaddrs[1]) ||
		    (pids[0] == pids[2] && vaddrs[0] == vaddrs[2]) ||
		    (pids[1] == pids[2] && vaddrs[1] == vaddrs[2])) {
			printf("NOTICE - cpu %d has duplicate tlb entries (%d, %d).\n", id, i, j);
			return;
		}
	}
#endif
}

void
checkAllTlbsDumped()
{
	int	i;
	tlbinfo_t *tinfo;

        if (tlbdumptbl == (tlbinfo_t *)0) /* Array not allocated ?? */
                return;

	for (i=0; i < maxcpus; i++) {
		if (!cpu_enabled(i))
			continue;
		tinfo = &tlbdumptbl[i];
		if (tinfo->tlbentry_valid == 0)
			printf("NOTICE - cpu %d didn't dump TLB, may be hung\n", i);
	}
}
