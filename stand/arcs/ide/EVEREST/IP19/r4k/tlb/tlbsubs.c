
/*
 * cpu/cpusubs.c
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.6 $"


#include <sys/param.h>
#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/immu.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <values.h>
/* #include <parser.h> */
#include "pattern.h"
#include "ip19.h"
#include "uif.h"
#include "memstr.h"
#include "tlb.h"

/*
#define DEBUG	0
*/


tlbptepairs_t wpte[NTLBENTRIES];  /* each tlb entry maps even+odd vpage */
unsigned char *k0a, *k1a, *pa;


/* from cpu/vm.c -- This is the bringup version of TLB functionality test.
 * The TLB is set up to map to addresses beginning at PHYS_FILL_LO.
 */
void pte_setup(void)
{
	register unsigned char *p;
	register int index, pgnum;

	p = pa = (unsigned char *)(PHYS_FILL_LO & TLBHI_VPNMASK);

	k1a = (unsigned char *)PHYS_TO_K1(pa);
	k0a = (unsigned char *)PHYS_TO_K0(pa);

	/* slot-indexes vary from 0-->NTLBENTRIES-1, pgnums vary by 2's
	 * because each entry maps even+odd page */
	for (index = 0,pgnum = 0; index < NTLBENTRIES; index++,pgnum+=2) {
		uc_bwrite(k1a+(pgnum*NBPP), pgnum);
		uc_bwrite(k1a+((pgnum+1)*NBPP), pgnum+1);
		wpte[index].evenpg =
(uint)((((__psunsigned_t)p+(pgnum*NBPP)) >> TLBHI_VPNSHIFT) << TLBLO_PFNSHIFT);
	    wpte[index].oddpg =
(uint)((((__psunsigned_t)p+((pgnum+1)*NBPP)) >> TLBHI_VPNSHIFT) << TLBLO_PFNSHIFT);
	}
}

void flushall_tlb(void)
{
	register int i;

	if (console_is_gfx())
	    i = 2;
	else
	    i = 0;
	do {
		invaltlb(i);
		i++;
	} while ( i < NTLBENTRIES );
	clr_tlb_regs();
}

