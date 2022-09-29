
/*
 * tlb_probe.c
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

#ident "$Revision: 1.8 $"


/*
 * See if all the tlb slots respond to probes upon address match.
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include "everr_hints.h"
#include "ide_msg.h"
#include "pattern.h"
#include "tlb.h"
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

int
ide_tlb_probe(void)
{
	register i, startent, indx, vpnum;
	unsigned retval=0;

	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (tlb_probe) \n");
	msg_printf(VRB, "Running CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);

	if (console_is_gfx())
	{
	    startent = 2;
	    msg_printf(INFO, "TLB entries 0 and 1 skipped. Can be tested only when run on an ASCII console\n");
	}
	else
	    startent = 0;
	ta_spl();
	pte_setup();
	flushall_tlb();

	/* set tlbpid to 0 */
	set_tlbpid(0);

	for (i = startent,vpnum = 0; i < NTLBENTRIES; i++,vpnum+=2) {
		settlbslot(i, 0, vpnum*NBPP,
				wpte[i].evenpg|TLBLO_V|TLBLO_NONCOHRNT,
				wpte[i].oddpg|TLBLO_V|TLBLO_NONCOHRNT);

		/* get index register */
		indx = matchtlb(0, vpnum);
		if (indx != i) {	/* failed to find a match */
			err_msg(TLBPR_ERR, cpu_loc, i, indx, vpnum, (vpnum*NBPP));
			show_fault();
			retval = 1;
		}
		invaltlb(i);
	}
	flushall_tlb();
	msg_printf(INFO, "Completed TLB probe test\n");
	return(retval);
}
