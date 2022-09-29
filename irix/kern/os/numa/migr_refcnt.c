/*****************************************************************************
 * Copyright 1997, Silicon Graphics, Inc.
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
#include <sys/proc.h>
#include <sys/runq.h>
#include <sys/rt.h>
#include <sys/sat.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/idbgentry.h>
#include <sys/sysmacros.h>
#include <sys/pda.h>
#include <sys/timers.h>
#include <ksys/sthread.h>
#include <sys/schedctl.h>
#include <sys/nodepda.h>
#include "pfms.h"
#include "migr_periodic_op.h"
#include "migr_bounce.h"
#include "debug_levels.h"
#include "numa_init.h"
#include "migr_control.h"
#include "migr_queue.h"
#include "migr_manager.h"
#include "numa_hw.h"
#include "numa_stats.h"
#include "migr_unpegging.h"
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/hwgraph.h>
#include <sys/SN/hwcntrs.h>
#include "migr_refcnt.h"

/*
 * We have one extended-counters buffer per node. This buffer is placed in
 * contiguous physical memory, and it is structured as follows:
 *
 * ---------------------------------        -                            -
 *| Counter[0] for hwpfn 0          |        |                            |
 * ---------------------------------         |                            |
 *| Counter[1] for hwpfn 0          |        |                            |
 * ---------------------------------         | Counters for hwpfn 0       |
 *| ...                             |        |   SET of Counters          |
 * ---------------------------------         |                            |
 *| Counter[numnodes-1] for hwpfn 0 |        |                            |
 *----------------------------------        -                             |
 *| Counter[0] for hwpfn 1          |        |                            |
 * ---------------------------------         |                            |
 *| Counter[1] for hwpfn 1          |        |                            |
 * ---------------------------------         | Counters for hwpfn 1       |
 *| ...                             |        |                            |
 * ---------------------------------         |                            |
 *| Counter[numnodes-1] for hwpfn 1 |        |                            |
 *----------------------------------        -                             | Counters for swpfn [i]
 *| Counter[0] for hwpfn 2          |        |                            |  SUPERSET of counters
 * ---------------------------------         |                            |
 *| Counter[1] for hwpfn 2          |        |                            |
 * ---------------------------------         | Counters for hwpfn 2       |
 *| ...                             |        |                            |
 * ---------------------------------         |                            |
 *| Counter[numnodes-1] for hwpfn 2 |        |                            |
 *----------------------------------        -                             |
 *| Counter[0] for hwpfn 3          |        |                            |
 * ---------------------------------         |                            |
 *| Counter[1] for hwpfn 3          |        |                            |
 * ---------------------------------         | Counters for hwpfn 3       |
 *| ...                             |        |                            |
 * ---------------------------------         |                            |
 *| Counter[numnodes-1] for hwpfn 3 |        |                            |
 *----------------------------------        -                            -
 *
 * This pattern repeats for each swpfn in a node
 *
 * We also have an array of slots that point to the sections of the extended-counters
 * buffer array that correspond to the beginning of a slot of physical memory:
 *
 *  ---------------------------------------------
 * | Pointer to Start of Slot 0 in counter buffer|
 *  ---------------------------------------------
 * | Pointer to Start of Slot 1 in counter buffer|
 *  ---------------------------------------------
 * | Pointer to Start of Slot 2 in counter buffer|
 *  ---------------------------------------------
 * | ....                                        |
 *  ---------------------------------------------
 * | Pointer to Start of Slot numslots-1         |
 *  ---------------------------------------------
 *
 */


/*
 * A `SET' is defined as the set of counters
 * associated with one hwpfn. This is equivalent
 * to numnodes.
 */
#define MIGR_REFCNT_COUNTERS_PER_SET  (numnodes)

/*
 * There are NUM_OF_HW_PAGES_PER_SW_PAGE() `SETs'
 * in each `SUPERSET'
 */
#define MIGR_REFCNT_SETS_PER_SUPERSET (NUM_OF_HW_PAGES_PER_SW_PAGE())

/*
 * A `SUPERSET' is defined as the set of counters
 * associated with one swpfn. Each swpfn
 * has  MIGR_REFCNT_SETS_PER_SUPERSET hwpfns,
 * and there are  MIGR_REFCNT_COUNTERS_PER_SET counters for each hwpfn.
 */
#define MIGR_REFCNT_COUNTERS_PER_SUPERSET (MIGR_REFCNT_COUNTERS_PER_SET * MIGR_REFCNT_SETS_PER_SUPERSET)

/*
 * The counters in a SUPERSET correspond to exactly one page
 */
#define MIGR_REFCNT_COUNTERS_PER_PAGE MIGR_REFCNT_COUNTERS_PER_SUPERSET


static refcnt_t*
migr_refcnt_supersetbase_get(cnodeid_t home_node, pfn_t swpfn)
{
        refcnt_t* lcbase;
        lcbase = (((refcnt_t**)(NODEPDA(home_node)->migr_refcnt_counterbase))[pfn_to_slot_num(swpfn)]);
        ASSERT(NODEPDA(home_node)->migr_refcnt_counterbuffer <= lcbase);
        ASSERT((char*)lcbase < ((char*)NODEPDA(home_node)->migr_refcnt_counterbuffer + NODEPDA(home_node)->migr_refcnt_cbsize));
        lcbase += pfn_to_slot_offset(swpfn) * MIGR_REFCNT_COUNTERS_PER_PAGE;
        return (lcbase);
}

/*
 * Allocate memory for extended memory reference counters
 */

int
migr_refcnt_init(cnodeid_t node)
{
        refcnt_t**  array_of_slots = NULL;
        refcnt_t*   counter_buffer = NULL;
        refcnt_t*   next_counter_buffer = NULL;
        int slot;
        int nslots;
	int slot_psize;
        int total_pages_in_node;
        int counter_buffer_size;
        
        ASSERT(node >= 0 && node < numnodes);

#ifdef DEBUG_REFCNT
        printf("migr_refcnt_init for cnodeid %d\n", node);
#endif        

#ifdef HUB_MIGR_WAR
        if ((CNODE_NUM_CPUS(node) > 0) && WAR_MD_MIGR_ENABLED(node)) {
                return (2);
        }
#endif 
                
        /*
         * Allocate memory for the counters
         */

        /*
         * First we determine how many pages we have in this node,
         * in order to allocate a contiguous array for the extended
         * counters.
         */

        total_pages_in_node = 0;
        nslots = node_getnumslots(node);
        for (slot = 0; slot < nslots; slot++) {
                int slot_size = slot_getsize(node,slot);
                total_pages_in_node += slot_size;
#ifdef DEBUG_REFCNT                
                printf("SLOT(%d) SIZE: %d\n", slot, slot_size);
#endif                
        }

#ifdef DEBUG_REFCNT
        printf("\t\tPages: %d\n", total_pages_in_node);
#endif

        /*
         * Now we allocate the actual array
         */

        counter_buffer_size = total_pages_in_node * MIGR_REFCNT_COUNTERS_PER_PAGE * sizeof(refcnt_t); 
        
        counter_buffer = kmem_contig_alloc_node(node,
                                                counter_buffer_size,
                                                64*1024,
                                                VM_NOSLEEP);
        if (counter_buffer == NULL) {
                goto fail;
        }

#ifdef DEBUG_REFCNT
        printf("counter_buffer: 0x%llx, size: %d, counter_buffer+size: 0x%llx\n",
               counter_buffer,  counter_buffer_size, (char*)counter_buffer + counter_buffer_size);      
#endif

        bzero(counter_buffer, counter_buffer_size);
        
        /*
         * Now we allocate access structures for the kernel.
         * Users mapping the counter buffer will setup their
         * own access structures.
         */
        
        nslots = node_getnumslots(node);
        array_of_slots = kmem_alloc_node(nslots * sizeof(refcnt_t*), KM_NOSLEEP, node);
        if (array_of_slots == NULL) {
                goto fail;
        }
        
        next_counter_buffer = counter_buffer;
        for (slot = 0; slot < nslots; slot++) {
		slot_psize = slot_getsize(node,slot);
		if (slot_psize > 0) {
                        ASSERT_ALWAYS(((char*)next_counter_buffer - (char*)counter_buffer) < counter_buffer_size);
#ifdef DEBUG_REFCNT                        
                        printf("Slot[%d]: buffer: 0x%llx, size: %d, buffer+size: 0x%llx\n",
                               slot,
                               next_counter_buffer,
                               slot_psize *  MIGR_REFCNT_COUNTERS_PER_PAGE * sizeof(refcnt_t),
                               (char*)next_counter_buffer +
                               slot_psize * MIGR_REFCNT_COUNTERS_PER_PAGE * sizeof(refcnt_t));
#endif                        
                  
                        array_of_slots[slot] = next_counter_buffer;
			next_counter_buffer += slot_psize * MIGR_REFCNT_COUNTERS_PER_PAGE;
                } else {
                        array_of_slots[slot] = NULL;
                }
        }

                
        NODEPDA(node)->migr_refcnt_counterbase = array_of_slots;
        NODEPDA(node)->migr_refcnt_counterbuffer = counter_buffer;
        NODEPDA(node)->migr_refcnt_cbsize = counter_buffer_size;
        NODEPDA(node)->migr_refcnt_numsets = total_pages_in_node * MIGR_REFCNT_SETS_PER_SUPERSET;
        
        /*
         * Set the absolute threshold for this node
         */
        MIGR_THRESHOLD_ABS_SET(node,
                               NODEPDA_MCD(node)->migr_system_kparms.migr_effthreshold_refcnt_overflow);
        /*
         * Enable absolute interrupts
         */
        MIGR_THRESHOLD_ABS_ENABLE(node);

        return (0);

  fail:

        /*
         * We need to release any allocated memory before
         * returning failure.
         */

        if (counter_buffer != NULL) {
                kmem_contig_free(counter_buffer, counter_buffer_size);
        }
        
	ASSERT(array_of_slots == NULL);

        NODEPDA(node)->migr_refcnt_counterbase = 0;
        NODEPDA(node)->migr_refcnt_counterbuffer = 0;
        NODEPDA(node)->migr_refcnt_cbsize = 0;
        NODEPDA(node)->migr_refcnt_numsets = 0;

        return (1);
}        

void
migr_refcnt_dealloc(cnodeid_t node)
{
        int nslots;
        
        if (NODEPDA(node)->migr_refcnt_counterbuffer != NULL) {
                ASSERT(NODEPDA(node)->migr_refcnt_cbsize != 0);

                kmem_contig_free(NODEPDA(node)->migr_refcnt_counterbuffer, NODEPDA(node)->migr_refcnt_cbsize);
                nslots = node_getnumslots(node);
                kmem_free(NODEPDA(node)->migr_refcnt_counterbase, nslots * sizeof(refcnt_t*));
                
                NODEPDA(node)->migr_refcnt_counterbase = NULL;
                NODEPDA(node)->migr_refcnt_counterbuffer = NULL;
                NODEPDA(node)->migr_refcnt_cbsize = 0;
                NODEPDA(node)->migr_refcnt_numsets = 0;
        }
}


void
migr_refcnt_get_offsets(cnodeid_t home_node,
                        pfn_t swpfn,
                        pfmsv_t pfmsv,
                        uint* home_ofs,
                        uint* remote_ofs,
                        uint* new_mode,
                        refcnt_t** cbase)
{
        uchar_t migr_user_threshold;
        uchar_t migr_node_threshold;

        ASSERT(home_ofs);
        ASSERT(remote_ofs);
        ASSERT(new_mode);

        if (PFMSV_IS_MIGR_ENABLED(pfmsv) && !PFMSV_IS_FROZEN(pfmsv)) {
                /*
                 * If migration is enabled, we are in relative mode, and the counters
                 * were pre-initialized with some offset.
                 */

                migr_user_threshold = PFMSV_MGRTHRESHOLD_GET(pfmsv);
                migr_node_threshold = NODEPDA_MCD(home_node)->migr_as_kparms.migr_base_threshold;

                if (migr_node_threshold > migr_user_threshold) {
                        /*
                         * The user is lowering the threshold
                         */
                        *remote_ofs = MIGR_DIFF_THRESHOLD_REL_TO_EFF(home_node,
                                                                     migr_node_threshold - migr_user_threshold);
                        *home_ofs = 0;
                } else {
                        /*
                         * The user is increasing the threshold.
                         */
                        *remote_ofs = 0;
                        *home_ofs = MIGR_DIFF_THRESHOLD_REL_TO_EFF(home_node,
                                                                   migr_user_threshold - migr_node_threshold);
                }
                *new_mode =  MD_PROT_MIGMD_IREL;

        } else if (PFMSV_IS_MIGRREFCNT_ENABLED(pfmsv)) {
                /*
                 * No migration and refcnt enabled: we're in abs mode
                 * No offsets
                 */
                *remote_ofs = 0;
                *home_ofs = 0;
                *new_mode =  MD_PROT_MIGMD_IABS;

        } else {
                *remote_ofs = 0;
                *home_ofs = 0;
                 *new_mode =  MD_PROT_MIGMD_OFF;
        }

        if (PFMSV_IS_MIGRREFCNT_ENABLED(pfmsv)) {
                ASSERT(PFN_TO_CNODEID(swpfn) == home_node);
                ASSERT(NODEPDA(home_node)->migr_refcnt_counterbase);
                ASSERT(NODEPDA(home_node)->migr_refcnt_counterbuffer);
                ASSERT(NODEPDA(home_node)->migr_refcnt_cbsize);

                *cbase = migr_refcnt_supersetbase_get(home_node, swpfn);
        } else {
                *cbase = NULL;
        }
}


void
migr_refcnt_update_counters(cnodeid_t home_node,
                            pfn_t swpfn,
                            pfmsv_t pfmsv,
                            uint new_mode)
{
	int i, j;
	pfn_t hwpfn;
        refcnt_t* cbase = NULL;
        refcnt_t count;
        uint home_ofs;
        uint remote_ofs;
        uint not_used;

	if (NODEPDA_MCD(home_node)->migr_as_kparms.migr_refcnt_mode
		 					== REFCNT_MODE_DIS)
		return;
        migr_refcnt_get_offsets(home_node, swpfn, pfmsv, &home_ofs, &remote_ofs, &not_used, &cbase);
 
        if (cbase) {

                /*
                 * We need to transfer the hardware counters into the
                 * extended software counters
                 */

                /*
                 * Organization of software counters:
                 * Counter for hwpfn+0 for node 0
                 * Counter for hwpfn+0 for node 1
                 * ....
                 * Counter for hwpfn+1 for node 0
                 * Counter for hwpfn+1 for node 1
                 * ....
                 */

                /*
                 * For every 4K page set of counters in a 16K page
                 */

                for (i = 0, hwpfn = SWPFN_TO_HWPFN_BASE(swpfn);
                     i < NUM_OF_HW_PAGES_PER_SW_PAGE();
                     i++,  hwpfn++, cbase+=numnodes) {
                
                        /*
                         * For every counter in the set associated with hwpfn
                         */
                        for (j = 0; j < numnodes; j++) {
                                /*
                                 * Get the current value
                                 */
                                count = MIGR_COUNT_GET(MIGR_COUNT_REG_GET(hwpfn, j), j);

                                /*
                                 * Update software counter
                                 * We are careful here to substract the initial value the
                                 * hardware counter was initialized to.
                                 */
                                if (j == home_node) {
                                        cbase[j] += count - home_ofs;
                                } else {
                                        cbase[j] += count - remote_ofs;
                                }

                               
                        }

                }
        }

        /*
         * Set the counters to the new requested mode
         */

        if (new_mode != MD_PROT_MIGMD_IREL) {
                home_ofs = remote_ofs = 0;
        }
        
        /*
         * For every 4K page set of counters in a 16K page
         */
        for (i = 0, hwpfn = SWPFN_TO_HWPFN_BASE(swpfn);
             i < NUM_OF_HW_PAGES_PER_SW_PAGE();
             i++,  hwpfn++) {
                
                /*
                 * For every counter in the set associated with hwpfn
                 */

                
                for (j = 0; j < numnodes; j++) {
#ifdef HUB_MIGR_WAR
                        if (CNODE_NUM_CPUS(home_node) && WAR_MD_MIGR_ENABLED(home_node)) {
                                if (j == home_node) {
                                        uchar_t migr_threshold = PFMSV_MGRTHRESHOLD_GET(pfmsv);
                                        MIGR_COUNT_REG_SET(hwpfn,
                                                           home_node,
                                                           MIGR_DIFF_THRESHOLD_REL_TO_EFF(j, migr_threshold),
                                                           (new_mode==MD_PROT_MIGMD_IREL)?MD_PROT_MIGMD_OFF:new_mode);
                                } else {
                                        MIGR_COUNT_REG_SET(hwpfn,
                                                           j,
                                                           0,
                                                           new_mode);
                                }
                        } else
#endif /* HUB_MIGR_WAR */
                        {
                                if (j == home_node) {
                                        MIGR_COUNT_REG_SET(hwpfn,
                                                           home_node,
                                                           home_ofs,
                                                           (new_mode==MD_PROT_MIGMD_IREL)?MD_PROT_MIGMD_OFF:new_mode);
                                } else {
                                        MIGR_COUNT_REG_SET(hwpfn,
                                                           j,
                                                           remote_ofs,
                                                           new_mode);
                                }
                        }

                }
                
        }
                
}

void 
migr_refcnt_restart(pfn_t swpfn, pfmsv_t pfmsv, uint reset)
{
	int i, j;
	pfn_t hwpfn;
        uint mode;
        uchar_t migr_threshold;
        uint remote_ofs;
        uint home_ofs;
        refcnt_t* cbase;
        
	cnodeid_t home_node = PFN_TO_CNODEID(swpfn);

	if (NODEPDA_MCD(home_node)->migr_as_kparms.migr_refcnt_mode
		 					== REFCNT_MODE_DIS)
		return;

        migr_threshold = PFMSV_MGRTHRESHOLD_GET(pfmsv);

        cbase = NULL;

        
        migr_refcnt_get_offsets(home_node,
                                swpfn,
                                pfmsv,
                                &home_ofs,
                                &remote_ofs,
                                &mode,
                                &cbase
                                );

        if (cbase && reset) {
                /*
                 * Reset the software counters
                 */
                
                for (i = 0, hwpfn = SWPFN_TO_HWPFN_BASE(swpfn);
                     i < NUM_OF_HW_PAGES_PER_SW_PAGE();
                     i++,  hwpfn++, cbase+=numnodes) {
                
                        /*
                         * For every counter in the set associated with hwpfn
                         */
                        for (j = 0; j < numnodes; j++) {
                                /*
                                 * Zero out extended counter
                                 */
                                cbase[j] = 0;
                        }
                }
        }
        

        hwpfn = SWPFN_TO_HWPFN_BASE(swpfn);

        for (i = 0; i < NUM_OF_HW_PAGES_PER_SW_PAGE(); i++) {

                for(j = 0; j < numnodes; j++) {
#ifdef HUB_MIGR_WAR
                        if (CNODE_NUM_CPUS(home_node) && WAR_MD_MIGR_ENABLED(home_node)) {
                                if (j == home_node) {
                                        MIGR_COUNT_REG_SET(hwpfn,
                                                           j,
                                                           MIGR_DIFF_THRESHOLD_REL_TO_EFF(j, migr_threshold),
                                                           (mode==MD_PROT_MIGMD_IREL)?MD_PROT_MIGMD_OFF:mode);

                                
                                } else {
                                        MIGR_COUNT_REG_SET(hwpfn,
                                                           j,
                                                           0,
                                                           mode);
                                }
                        } else
#endif /* HUB_MIGR_WAR */
                        {
                                if (j == home_node) {
                                        MIGR_COUNT_REG_SET(hwpfn,
                                                           j,
                                                           home_ofs,
                                                           (mode==MD_PROT_MIGMD_IREL)?MD_PROT_MIGMD_OFF:mode);
                                } else {
                                        MIGR_COUNT_REG_SET(hwpfn,
                                                           j,
                                                           remote_ofs,
                                                           mode);
                                }
                        }
                        
                }
                
                hwpfn++;
                
        }
}



/*
 * Read extended ref counters
 */
void
migr_refcnt_read_extended(sn0_refcnt_buf_t* inbuf)
{
        pfn_t swpfn;
        refcnt_t* cbase;
        cnodeid_t node;
        int hwpfn_offset;
        int counter;


        ASSERT(inbuf);
        swpfn = btoct(inbuf->paddr);
        node = PFN_TO_CNODEID(swpfn);
        hwpfn_offset = PHYSADDR_TO_HWPFN_OFFSET(inbuf->paddr);

#ifdef DEBUG_REFCNT
        printf("GET EXTREFCNT for swpfn 0x%x, node %d, hwpfn_offset %d\n",
               swpfn, node, hwpfn_offset);
#endif        

        ASSERT(node >= 0  && node < numnodes);
        ASSERT(pfntopfdat(swpfn) >= PFD_LOW(node)  && pfntopfdat(swpfn) <=  PFD_HIGH(node));


        cbase = migr_refcnt_supersetbase_get(node, swpfn);
        cbase += (MIGR_REFCNT_COUNTERS_PER_SET * hwpfn_offset);

        for (counter = 0; counter < numnodes; counter++) {
                inbuf->refcnt_set.refcnt[counter] = cbase[counter];
        }
        for (counter = numnodes; counter < SN0_REFCNT_MAX_COUNTERS; counter++) {
                inbuf->refcnt_set.refcnt[counter] = 0;
        }

#ifdef DEBUG_REFCNT       
        for (counter = 0; counter < numnodes; counter++) {
                printf("Counter[swpfn:0x%x, ofs:%d, for node %d] = %d\n",
                       swpfn, hwpfn_offset, counter, cbase[counter]);
        }
#endif
        
}

/*
 * Read hardware counters for SN0
 */

void
migr_refcnt_read(sn0_refcnt_buf_t* inbuf)
{
        __uint64_t paddr;
        pfn_t swpfn;
        pfn_t hwpfn;
        cnodeid_t node;

        ASSERT(inbuf);
        paddr = inbuf->paddr;
        swpfn = btoct(paddr);
        hwpfn = SWPFN_TO_HWPFN_BASE(swpfn) + PHYSADDR_TO_HWPFN_OFFSET(paddr);

        for (node = 0; node < numnodes; node++) {
                inbuf->refcnt_set.refcnt[node] =
                        MIGR_COUNT_GET(MIGR_COUNT_REG_GET(hwpfn, node), node);
        }
}


int
migr_refcnt_enabled()
{
	return (nodepda->mcd->migr_as_kparms.migr_refcnt_mode 
			!= REFCNT_MODE_DIS);
}
