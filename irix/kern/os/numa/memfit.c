/*****************************************************************************
 * Copyright 1996, Silicon Graphics, Inc.
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
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <sys/nodepda.h>
#include <sys/nodemask.h>
#include <sys/idbgentry.h>
#include <sys/space.h>
#include "os/scheduler/space.h"
#include "memfit.h"

extern int idbg_memfit_print(void);

memfit_t global_memfit;

void
memfit_init(void)
{
        int i;
        cnodeid_t node;
	int ncpusets;
        memfit_t* memfit = &global_memfit;

        for (i = 0; i < MEMP_NBUCKETS; i++) {
                CNODEMASK_CLRALL(memfit->mem_pressure.bucket[i]);
        }

        memfit->count = 0;

        for (node = 0; node < numnodes; node++) {
                NODEPDA(node)->memfit_assign = 0;
        }

        memfit->mem_mean_assign = 0;
	ncpusets = MAXCPUS + 2;
	for (i=0; i<ncpusets; i++)
        	memfit->nrotor[i] = numnodes-1;
        CNODEMASK_SETALL(memfit->mem_unassigned);
        
        idbg_addfunc("memfit", (void (*)())idbg_memfit_print);

}

#define freemem_to_bucket(max, level) ((max==0)?(MEMP_NBUCKETS-1):(((level) * MEMP_NBUCKETS) / (max)))

int
idbg_memfit_print(void)
{
        int bucket, j;
        memfit_t* memfit = &global_memfit;
        

        qprintf("MeanAssign: %d, Update Count: %d, ",
                memfit->mem_mean_assign, memfit->count);
	qprintf("Rotors: ");
	for (j=0; j<20; j++)		/* Only print the first few rotors */
		qprintf("  %d", memfit->nrotor[j]);
	qprintf("\n");
        
	qprintf("MemUnassigned: [");
	for (j=0; j<CNODEMASK_SIZE; j++)
            qprintf(" %016llx ", CNODEMASK_WORD(memfit->mem_unassigned, j));
	qprintf("\n");

        qprintf("Memory pressure (cm_freemem=%d)\n", memfit->mem_pressure.cm_freemem);
        for (bucket = 0; bucket < MEMP_NBUCKETS; bucket++) {
            qprintf(" BUCKET %d, baselv %5d , [",
                        bucket, memfit->mem_pressure.baselv[bucket]);
	    for (j=0; j<CNODEMASK_SIZE; j++)
                qprintf(" %016llx ",
                        CNODEMASK_WORD(memfit->mem_pressure.bucket[bucket], j));
	    qprintf("]\n");
        }


        return (0);
}

void
memfit_master_update(pfn_t cm_freemem)
{
        cnodeid_t node;
        cnodemask_t mask;
        int highest_bucket;
        int b;
        memfit_t* memfit =  &global_memfit;
        memfit_t tmp_memfit;
        uint acc_assign;

        ASSERT(memfit);

#ifdef DEBUG
        memfit->count++;
#endif /* DEBUG */        

        acc_assign = 0;
        tmp_memfit.mem_pressure.cm_freemem = cm_freemem;

	
        for (b = 0; b < MEMP_NBUCKETS; b++) {
                tmp_memfit.mem_pressure.baselv[b] = ((cm_freemem * b) / MEMP_NBUCKETS);
                CNODEMASK_CLRALL(tmp_memfit.mem_pressure.bucket[b]);
        }

        
        CNODEMASK_CLRALL(mask);
        for (node = 0, CNODEMASK_SETB(mask,0);
             node < numnodes;
             node++, CNODEMASK_SHIFTL_PTR(&mask)) {
                ASSERT(CNODEMASK_IS_NONZERO(mask));
                highest_bucket = freemem_to_bucket(cm_freemem, NODE_FREEMEM(node));
                if (highest_bucket > MEMP_NBUCKETS) {
                        highest_bucket = MEMP_NBUCKETS - 1;
                }
                for (b = highest_bucket; b >= 0; b--) {
                        CNODEMASK_SETM(tmp_memfit.mem_pressure.bucket[b], mask);
                }
                acc_assign += NODEPDA(node)->memfit_assign;
        }


#ifdef DEBUG
        {
		int i;
                CNODEMASK_CLRALL(mask);
                for (b = 0; b < MEMP_NBUCKETS; b++) {
                        CNODEMASK_SETM(mask, tmp_memfit.mem_pressure.bucket[b]);
                }
                if (!CNODEMASK_EQ(mask, CNODEMASK_BOOTED_MASK)) {
			cnodemask_t tmp_mask;

                        printf("MEMFIT UPDATE: Not all nodes accounted for, mask: ");
			for (i=0; i < CNODEMASK_SIZE; i++)
                            printf("0x%llx ", CNODEMASK_WORD(mask,i)); 
			printf("\n");

			printf("CNODEMASK_BOOTED_MASK: ");
			for (i=0; i < CNODEMASK_SIZE; i++)
                            printf("0x%llx ", CNODEMASK_WORD(CNODEMASK_BOOTED_MASK,i)); 
			printf("\n");

                        tmp_memfit.mem_pressure.cm_freemem = cm_freemem;

                        for (b = 0; b < MEMP_NBUCKETS; b++) {
                                tmp_memfit.mem_pressure.baselv[b] = ((cm_freemem * b) / MEMP_NBUCKETS);
                                CNODEMASK_CLRALL(tmp_memfit.mem_pressure.bucket[b]);
                        }
        
                        for (node = 0, tmp_mask = CNODEMASK_CVTB(0);
                             node < numnodes;
                             node++, CNODEMASK_SHIFTL_PTR(&tmp_mask)) {
                                ASSERT(CNODEMASK_IS_NONZERO(tmp_mask));
                                
                                printf("MF LOOP: node: %d, cm_freemem: %d, freemem: %d\n",
                                       node, cm_freemem, NODE_FREEMEM(node));
                                
                                highest_bucket = freemem_to_bucket(cm_freemem, NODE_FREEMEM(node));
                                
                                if (highest_bucket > MEMP_NBUCKETS) {
                                        highest_bucket = MEMP_NBUCKETS - 1;
                                }
                                
                                for (b = highest_bucket; b >= 0; b--) {
                                        printf("MF LOOP, node %d, hbucket %d, cmask: ",
                                               node, highest_bucket); 
				
					for (i=0; i < CNODEMASK_SIZE; i++)
                                           printf("0x%llx ",
						CNODEMASK_WORD(tmp_memfit.mem_pressure.bucket[b],i)); 
					printf("\n");
                                        CNODEMASK_SETM(tmp_memfit.mem_pressure.bucket[b], tmp_mask);
	
                                }
                                
                               
                                
                        }
                        
                        panic("MEMFIT UPDATE: Not all nodes accounted for, mask: 0x%llx\n", mask);
                }
        }
#endif /* DEBUG */


        memfit->mem_pressure.cm_freemem = tmp_memfit.mem_pressure.cm_freemem;

        for (b = 0; b < MEMP_NBUCKETS; b++) {
                memfit->mem_pressure.baselv[b] = tmp_memfit.mem_pressure.baselv[b];
                CNODEMASK_CPY(memfit->mem_pressure.bucket[b], tmp_memfit.mem_pressure.bucket[b]);
        }
        memfit->mem_mean_assign = acc_assign/numnodes;
}


cnodeid_t
memfit_selectnode(cnodemask_t inmask)
{
        int bucket;
        int b;
        int c;
        int delta;
        cnodemask_t* mem_pressure_bucket, cnodemask;
        memfit_t* memfit = &global_memfit;
        cnodeid_t selection;

	cnodemask = get_effective_nodemask(curthreadp);
	CNODEMASK_ANDM(cnodemask, inmask);

	if(CNODEMASK_IS_ZERO(cnodemask))
		return CNODEID_NONE;

        while (1) {
                /*
                 * Choose the middle bucket
                 */
                bucket = MEMP_NBUCKETS / 2;

                mem_pressure_bucket = memfit->mem_pressure.bucket;

                b = bucket;
                delta = 1;
                for (c = 0; c < MEMP_NBUCKETS; c++) {
			cnodemask_t tmpmask;
                        ASSERT(b >= 0 && b < MEMP_NBUCKETS);

			CNODEMASK_CPY(tmpmask, mem_pressure_bucket[b]);
			CNODEMASK_ANDM(tmpmask, cnodemask);
                        if ((selection = memfit_unassigned_getnode(tmpmask)) != CNODEID_NONE) {
                                return (selection);
                        }

                        /*
                         * Try next bucket if we still have unassigned nodes;
                         * Otherwise, just break.
                         */
                        if (CNODEMASK_IS_ZERO(memfit->mem_unassigned)) {
                                break;
                        }

                        b += delta;
                        if (b == MEMP_NBUCKETS) {
                                b = bucket - 1;
                                delta = -1;
                        }

                }
                
        	CNODEMASK_SETALL(memfit->mem_unassigned);

        }


}

cnodeid_t
memfit_unassigned_getnode(cnodemask_t allow_mask)
{
        cnodeid_t node;
        memfit_t* memfit = &global_memfit;
	int	  kcs = curthreadp->k_cpuset;
	int 	  n;

        for (n=0, node = memfit->nrotor[kcs]; n < numnodes; n++) {
		if (CNODEMASK_TSTB(memfit->mem_unassigned,node) &&
		    CNODEMASK_TSTB(allow_mask,node)) {
			CNODEMASK_CLRB(memfit->mem_unassigned, node);
                        memfit->nrotor[kcs] = (node == numnodes-1) ? 0 : node+1;
                        NODEPDA(node)->memfit_assign++;
                        return (node);
                }
		node = (node == numnodes-1) ? 0 : node+1;
        }

        return (CNODEID_NONE);
}
                        
