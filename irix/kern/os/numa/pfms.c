/*****************************************************************************
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 ****************************************************************************/

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/runq.h>
#include <sys/sat.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/idbgentry.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include "pfms.h"
#include "migr_control.h"
#include "debug_levels.h"

/*
 * Allocate and initialize the page frame migration state table
 */

pfms_t*
nodepda_pfms_alloc(cnodeid_t node, caddr_t *nextbyte)
{
        pfms_t* pfmsp;
	int num_slots;
        int slot;
	int slot_psize;
	int alloc_size;

	/*REFERENCED*/
        caddr_t previous_nb;

        previous_nb = *nextbyte;
        
	pfmsp = (pfms_t*) low_mem_alloc(MAX_MEM_SLOTS * sizeof(pfms_t),
				        nextbyte, "pfmsp");

#ifdef DEBUG_PFMS        
        printf("[nodepda_pfms_alloc]: pfmsp: 0x%llx, to 0x%llx, len 0x%x, nextbytediff: 0x%x\n",
               pfmsp, pfmsp + MAX_MEM_SLOTS,
               MAX_MEM_SLOTS * sizeof(pfms_t), *nextbyte - previous_nb);
#endif /* DEBUG_PFMS */        
	
	ASSERT(pfmsp);

        bzero(pfmsp, MAX_MEM_SLOTS * sizeof(pfms_t)); 

	num_slots = node_getnumslots(node);

#ifdef DEBUG_PFMS
        printf("[nodepda_pfms_alloc]: number of slots: %d\n", num_slots);
#endif

	for (slot = 0; slot < num_slots; slot++) {
		slot_psize = slot_getsize(node, slot);
		if (slot_psize == 0)
			continue;
		alloc_size = slot_psize * sizeof(pfmsv_t);
	
		previous_nb = *nextbyte;
		
		pfmsp[slot] = (pfms_t) low_mem_alloc(alloc_size, nextbyte,
						"pfmsp[slot]");

#ifdef DEBUG_PFMS                                
		printf("[nodepda_pfms_alloc], pfms subarray for <slot %d>\n",
		       slot);
		printf("[nodepda_pfms_alloc] pfmsp[%d] is 0x%llx to 0x%llx, len: 0x%x, nbdiff 0x%x\n",
		       slot,
		       pfmsp[slot],
		       pfmsp[slot] + slot_psize,
		       alloc_size,
		       *nextbyte - previous_nb);
#endif /* DEBUG_PFMS */                                
		       
		bzero(pfmsp[slot], alloc_size);
	}

	return(pfmsp);
}








