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

/*
 * Operations on the queue used for migration control
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/numa.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/lpage.h>
#include <ksys/migr.h>
#include "pfms.h"
#include "migr_control.h"
#include "migr_manager.h"
#include "migr_states.h"
#include "migr_queue.h"
#include "migr_engine.h"
#include "debug_levels.h"
#include "numa_stats.h"


/*
 * Queue max len
 */
#define MIGR_QUEUE_MAXLEN (nodepda->mcd->migr_system_kparms.migr_queue_maxlen)

/*
 * Relative <->  abs capacity threshold
 */

#define MIGR_QUEUE_CAP_ABS_TO_REL(abs_value) \
        ( ((abs_value) * 100) / MIGR_QUEUE_MAXLEN )

#define MIGR_QUEUE_CAP_REL_TO_ABS(rel_value) \
        ( ((rel_value) * MIGR_QUEUE_MAXLEN) / 100 )


/* 
 * Initialization of the queue, called at startup time
 */

migr_queue_t*
migr_queue_alloc(size_t queue_size, cnodeid_t nodeid, caddr_t* nextbyte)
{
	migr_queue_t* new_queue;


	new_queue = (migr_queue_t*) low_mem_alloc(sizeof(migr_queue_t),
					nextbyte, "migr_queue_alloc");
        ASSERT (new_queue);
        
	new_queue->data = (migr_queue_item_t*)
                low_mem_alloc(sizeof(migr_queue_t) * queue_size,
				nextbyte, "new_queue->data");
	ASSERT(new_queue->data);

	/* 
	 * No items yet 
	 */
	new_queue->tail = new_queue->head = -1;

        /*
         * Initialize migr queue age
         * used to trigger migration due to time.
         */
        new_queue->age = 0;

        /*
         * Owner node
         */
        new_queue->source_nodeid = nodeid;

	spinlock_init(&new_queue->lock, "migr_queue_lock");

	return(new_queue);
}

/*ARGSUSED*/
static void
migr_queue_process(migr_queue_t* migr_queue)
{;}

        
/*
 * Age the queue, and check whether it's time to migrate it
 */

void 
migr_periodic_queue_control(migr_queue_t* migr_queue)
{
	int s;

	s = MIGRQUEUE_LOCK(migr_queue);

        /*
         * Age the queue if it's not empty
         */
        if (migr_queue->head != -1) {
                migr_queue->age++;
	}
                
        /*
         * Time migration trigger
         */
         if (nodepda->mcd->migr_system_kparms.migr_queue_time_trigger_enabled &&
             migr_queue->age >= nodepda->mcd->migr_system_kparms.migr_queue_time_trigger_interval) {
                 migr_queue->age = 0;
                 MIGRQUEUE_UNLOCK(migr_queue, s);
                 NUMA_STAT_INCR(cnodeid(), migr_queue_time_trigger_count);
                 migr_queue_process(migr_queue);
         }  else {
                 MIGRQUEUE_UNLOCK(migr_queue, s);
         }
        
}
	


/* ----------------------------------------------------------------------

   operations on queue
   
   ---------------------------------------------------------------------- */

/* 
 * Insertion of elements int he queue
 */
void
migr_queue_insert(migr_queue_t* migr_queue,
                  pfms_t pfms,
                  pfn_t swpfn, 
                  cnodeid_t dest_nodeid)
{
	int pfms_spl;
        int queue_spl;

	queue_spl = MIGRQUEUE_LOCK(migr_queue);

        /*
         * Before we insert the new page into the queue,
         * we need to verify whether it's still a migratable page
         */
        PFMS_LOCK(pfms, pfms_spl);
        
        if (!PFMS_STATE_IS_MIGRED(pfms)) {
                /*
                 * The page is not migratable anymore
                 */
                NUMA_STAT_INCR(migr_queue->source_nodeid, migr_queue_number_failstate);
                goto out;
        }

        /*
         * Mark the page a being queued for migration
         */

        PFMS_STATE_QUEUED_SET(pfms);

        if (migr_queue->head == -1) { /* the queue is empty */
                ASSERT(migr_queue->tail == -1);
                migr_queue->tail = migr_queue->head = 0;
        }
        else {
                if (migr_queue->numitems >= MIGR_QUEUE_MAXLEN) {
                        ASSERT(((migr_queue->tail + 1) % MIGR_QUEUE_MAXLEN) == migr_queue->head);
                        /* 
                         * Overflow
                         * We ignore this enqueue request,
                         * and move the page back to STATE_MAPPED.
                         */
                        PFMS_STATE_MAPPED_SET(pfms);
                        NUMA_STAT_INCR(migr_queue->source_nodeid, migr_queue_number_overflow);
                        goto out;
                }

                migr_queue->tail++;
                migr_queue->tail %= MIGR_QUEUE_MAXLEN;
                
        }

        /* 
         * Fill the data 
         */
        migr_queue->data[migr_queue->tail].swpfn = swpfn;
        migr_queue->data[migr_queue->tail].pfms = pfms;
        migr_queue->data[migr_queue->tail].dest_nodeid = dest_nodeid;

        migr_queue->numitems++;
        NUMA_STAT_INCR(migr_queue->source_nodeid, migr_auto_number_queued); 

        ASSERT(migr_queue->numitems <= MIGR_QUEUE_MAXLEN);

        if (nodepda->mcd->migr_system_kparms.migr_queue_capacity_trigger_enabled &&
            MIGR_QUEUE_CAP_ABS_TO_REL(migr_queue->numitems) >=
            nodepda->mcd->migr_system_kparms.migr_queue_capacity_trigger_threshold) {
                NUMA_STAT_INCR(migr_queue->source_nodeid, migr_queue_capacity_trigger_count);
                migr_queue_process(migr_queue);
        }

  out:
        PFMS_UNLOCK(pfms, pfms_spl);
        MIGRQUEUE_UNLOCK(migr_queue, queue_spl);
}
                                                       
void
migr_queue_migrate(migr_queue_t* migr_queue)
{
	int head;
	pfn_t swpfn;
	migr_page_list_t* mpl;
        migr_page_list_t* start_mpl;
	int queue_spl;
        int pfms_spl;
        pfms_t pfms;
        int item;
        int queueitems;
	int listitems = 0;
        int failitems = 0;
        

        /*
         * This method MUST be called from a kthread context
         */
        
	queue_spl = MIGRQUEUE_LOCK(migr_queue);

	start_mpl = mpl = NODEPDA_MCD(migr_queue->source_nodeid)->local_migr_page_list;

	head = migr_queue->head;
        queueitems = migr_queue->numitems;

        ASSERT(migr_queue->numitems >= 0 && migr_queue->numitems <= MIGR_QUEUE_MAXLEN); 

        for (item = 0; item < queueitems; item++) {
		swpfn = migr_queue->data[head].swpfn;
                pfms = migr_queue->data[head].pfms;
                
                PFMS_LOCK(pfms, pfms_spl);
                
                if (PFMS_STATE_IS_QUEUED(pfms)) {

                        /*
                         * put the page in the migration list
                         */
                        PFMS_STATE_MIGRED_SET(pfms);
			mpl->old_pfd = pfntopfdat(swpfn);
			mpl->new_pfd = NULL;
			mpl->dest_node = migr_queue->data[head].dest_nodeid;
			mpl->migr_err = FALSE;
			mpl++;
                        listitems++;
                } else {
                        failitems++;
                        NUMA_STAT_INCR(migr_queue->source_nodeid, migr_queue_number_failq);
                }

                PFMS_UNLOCK(pfms, pfms_spl);

                migr_queue->numitems--;
                head++;
                head %= MIGR_QUEUE_MAXLEN;
	}

        ASSERT(migr_queue->numitems == 0);
        ASSERT((listitems + failitems) == queueitems);
        migr_queue->tail = migr_queue->head = -1;
               

        MIGRQUEUE_UNLOCK(migr_queue, queue_spl);

	if (listitems) {
                int i;
                migr_migrate_page_list(start_mpl, listitems, CALLER_AUTOMIGR);
                for (i = 0, mpl = start_mpl; i < listitems; i++, mpl++) {
                        if (mpl->migr_err) {
                                /*
                                 * We have to restart the old pfn
                                 */
                                swpfn = pfdattopfn(mpl->old_pfd);
                                pfms = PFN_TO_PFMS(swpfn);
                                migr_restart(swpfn, pfms);
                                NUMA_STAT_INCR(cnodeid(), migr_queue_number_fail);
                        } else {
                                /*
                                 * We have to restart the new pfn
                                 */
                                swpfn = pfdattopfn(mpl->new_pfd);
                                pfms = PFN_TO_PFMS(swpfn);
                                migr_restart_clear(swpfn, pfms);
                                NUMA_STAT_INCR(cnodeid(), migr_queue_number_out);
				NUMA_STAT_INCR(mpl->dest_node, migr_queue_number_in);
                        }
                }
        }

}

#ifdef DEBUG

/* ----------------------------------------------------------------------

   Debugging functions
   
   ---------------------------------------------------------------------- */

/*
 * print the content of the queue
 * Sync: not needed since called only from symmon.
 */

void 
idbg_migr_queue_print(migr_queue_t* migr_queue)
{
	int i;
	int num_of_items;

	if (migr_queue->head == -1) { /* the queue is empty */
		ASSERT(migr_queue->tail == -1);
		qprintf("0 items\n");
	} else {
                /* The queue is not empty */
                num_of_items = migr_queue->numitems;

		qprintf("%d items\n", num_of_items);

		for (i = 0; i < num_of_items; i++) {
			qprintf("\t\t\t%d (pfn %d node %d)\n", 
				(migr_queue->head + i) % MIGR_QUEUE_MAXLEN,
                                migr_queue->data[(migr_queue->head + i) % MIGR_QUEUE_MAXLEN].swpfn,
				migr_queue->data[(migr_queue->head + i) % MIGR_QUEUE_MAXLEN].dest_nodeid);
		}


		if((migr_queue->head + i - 1) % MIGR_QUEUE_MAXLEN !=  migr_queue->tail) {
			qprintf("queue integrity problem\n");
		}
	}
}

#endif /* DEBUG */
