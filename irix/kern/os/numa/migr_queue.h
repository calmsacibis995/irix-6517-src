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

#ifndef __MIGR_QUEUE_H__
#define __MIGR_QUEUE_H__

typedef struct migr_queue_item_s {
	pfn_t     swpfn;             /* Number of the page to be migrated */
        pfms_t    pfms;              /* Page frame migration state */
	cnodeid_t dest_nodeid;       /* Node id of migration destination */
} migr_queue_item_t;

typedef struct migr_queue_s {
	migr_queue_item_t* data;     /* linked list of pages to migrate */
	short              head;     /* queue head */
	short              tail;     /* queue tail */
        short              age;      /* queue age (time) since last migr */
        cnodeid_t          source_nodeid; /* owner node */
        short              numitems; /* number of enqueued pages */
	lock_t             lock;     /* queue management lock */
} migr_queue_t;


/*
 * Locking on the queue
 */

#define MIGRQUEUE_LOCK(migr_queue)      (mutex_spinlock(&((migr_queue)->lock)))
#define MIGRQUEUE_UNLOCK(migr_queue, s) (mutex_spinunlock(&((migr_queue)->lock), (s)))


/*
 * Exported functions 
 */
extern migr_queue_t*
migr_queue_alloc(size_t queue_size, cnodeid_t nodeid, caddr_t* nextbyte);

extern void 
migr_periodic_queue_control(migr_queue_t* migr_queue);

extern void
migr_queue_insert(migr_queue_t* migr_queue,
                  pfms_t pfms,
                  pfn_t swpfn, 
                  cnodeid_t dest_nodeid);

extern void
migr_queue_migrate(migr_queue_t* migr_queue);

#ifdef DEBUG
extern void 
idbg_migr_queue_print(migr_queue_t* migr_queue);
#endif /* DEBUG */

#endif /* __MIGR_QUEUE_H__ */









