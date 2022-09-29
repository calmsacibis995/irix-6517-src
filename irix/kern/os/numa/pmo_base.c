/*
 * os/numa/pmo_base.c
 *
 *
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
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/pfdat.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/proc.h>
#include <sys/pmo.h>
#include <sys/mmci.h>
#include "pmo_base.h"
#include "pmo_error.h"
#include "sys/atomic_ops.h"

#ifdef MMCI_REFCD
refcd_trace_t refcd_trace;
int mmci_refd_counter = 0;

/*
 * Initialize refcd tracing
 */
void
pmo_base_refcd_trace_init(void)
{
        int i;
        
        refcd_trace.index = 0;
        refcd_trace.active = 0;
        spinlock_init(&refcd_trace.lock, "refcd");

        for (i = 0; i < REFCD_TRACE_LEN; i++) {
                refcd_trace.record[i].pmo = NULL;
                refcd_trace.record[i].rop_info = NULL;
        }
        mmci_refd_counter = 0;
        refcd_trace.active = 1;
}

/*
 * Find pmo in trace.
 * Returns the index into the trace, or -1 if not found.
 */
static int
pmo_base_refcd_trace_find(void* pmo)
{
        int i;
        
        for (i = 0; i < refcd_trace.index; i++) {
                if (refcd_trace.record[i].pmo == pmo) {
                        return (i);
                }
        }
        return (-1);
}


/*
 * To insert tracing record when a pmo is created
 */
static void
pmo_base_refcd_trace_insert(void* pmo)
{
        int s;
        int loc;

        ASSERT(pmo);

        if (!refcd_trace.active) {
                ((pmo_base_t*)pmo)->refcdi = -1; 
                return;
        }
        
        s = pmo_base_refcd_trace_lock();
        /*
         * Verify the object is not there
         */
        if (pmo_base_refcd_trace_find(pmo) >= 0) {
                /*
                 * Object alrady here...
                 */
                printf("Object 0x%llx already in trace\n", pmo);
                pmo_base_refcd_trace_unlock(s);
                return;
        }

        if (refcd_trace.index > REFCD_TRACE_LEN) {
                panic("pmo_base_refcd_trace_insert: trace overflow");
        }

        if (refcd_trace.index == REFCD_TRACE_LEN) {
                /*
                 * No more space
                 */
                refcd_trace.active = 0;
                pmo_base_refcd_trace_unlock(s);
                printf("REFCD TRACING HAS BEEN DEACTIVATED...\n");
                return;
        }
        
        loc = refcd_trace.index++;
        refcd_trace.record[loc].pmo = pmo;
        ((pmo_base_t*)pmo)->refcdi = loc;

        pmo_base_refcd_trace_unlock(s);
}

/*
 * To remove a pmo entry when the object is destroyed
 */
static void
pmo_base_refcd_trace_remove(pmo_base_t* pmo)
{
        int s;
        int loc;
        rop_info_t* p;
        rop_info_t* victim;
        
        if (!refcd_trace.active) {
                return;
        }

        s = pmo_base_refcd_trace_lock();

        ASSERT(pmo);

        loc = pmo->refcdi;
        if (loc == -1) {
                /*
                 * Created while tracing was not active
                 */
                pmo_base_refcd_trace_unlock(s);
                return;
        }
                
        ASSERT(loc >= 0 && loc < REFCD_TRACE_LEN);
        if (refcd_trace.record[loc].pmo != pmo) {
                printf("trace_remove:FATAL ERROR: Invalid pmo reference!!!\n");
                printf("loc: %d, refcd_trace.record[loc].pmo: 0x%llx, pmo: 0x%llx",
                       loc, refcd_trace.record[loc].pmo, pmo);
                pmo_base_refcd_trace_unlock(s);
                return;
        }
                

        ASSERT(refcd_trace.record[loc].pmo == pmo);

        for (p = refcd_trace.record[loc].rop_info; p != NULL; ) {
                victim = p;
                p = p->next;
                kmem_free(victim, sizeof(rop_info_t));
        }
        refcd_trace.record[loc].pmo = NULL;
        refcd_trace.record[loc].rop_info = NULL;

        pmo_base_refcd_trace_unlock(s); 
}
        

/*
 * Add refcout operation rop
 */
static void
pmo_base_refcd_trace_addrop(void* pmo, void* rop, char* file, int line, int refc, int op)
{
        rop_info_t* rop_info;
        pmo_base_t* pmob = (pmo_base_t*)pmo;
        int loc;
        int s;

        atomicAddInt(&mmci_refd_counter, op);
        
        if (!refcd_trace.active) {
                return;
        }
        
        if ((rop_info = kmem_zalloc(sizeof(rop_info_t), KM_NOSLEEP)) == NULL) {
                panic("pmo_base_refcd_trace_addrop: out of memory");
        }
        rop_info->rop = rop;
        rop_info->file = file;
        rop_info->line = line;
        rop_info->refcount = refc;
        rop_info->refcop = op;

        s = pmo_base_refcd_trace_lock();

        ASSERT(pmob);

        loc = pmob->refcdi;
        if (loc == -1) {
                /*
                 * Created while tracing was not active
                 */
                pmo_base_refcd_trace_unlock(s);
                return;
        }        
        ASSERT(loc >= 0 && loc < REFCD_TRACE_LEN);
        ASSERT(refcd_trace.record[loc].pmo == pmo);
        
        rop_info->next = refcd_trace.record[loc].rop_info;
        refcd_trace.record[loc].rop_info = rop_info;

        pmo_base_refcd_trace_unlock(s);
}
        
void
pmo_base_refcd_trace_activate(void)
{
        refcd_trace.active = 1;
}

void
pmo_base_refcd_trace_stop(void)
{
        refcd_trace.active = 0;
}

void
pmo_base_refcd_trace_clean(void)
{
        int i;
        pmo_base_t* pmo;
        rop_info_t* p;
        rop_info_t* victim;
        
        refcd_trace.active = 0;
        
        for (i = 0; i < REFCD_TRACE_LEN; i++) {
                pmo =  (pmo_base_t*)refcd_trace.record[i].pmo;
                if (pmo == NULL) {
                        continue;
                }
                pmo->refcdi = -1;
                
                for (p = refcd_trace.record[i].rop_info; p != NULL; ) {
                        victim = p;
                        p = p->next;
                        kmem_free(victim, sizeof(rop_info_t));
                }
                refcd_trace.record[i].pmo = NULL;
                refcd_trace.record[i].rop_info = NULL;
        }

        refcd_trace.index = 0;
        mmci_refd_counter = 0;
}
                
        
#endif /* MMCI_REFCD */

/**************************************************************************
 *                           PMO Base Methods                             *
 **************************************************************************/

#ifdef MMCI_REFCD

static void
pmo_base_incref(void* pmo_arg, void* rop, char* file, int line)
{
        pmo_base_t* pmo = (pmo_base_t*)pmo_arg;
        int refc;
        
        ASSERT(pmo);
  
        refc = atomicAddInt(&pmo->refcount, 1);

        ASSERT(rop);
        ASSERT(file);
        pmo_base_refcd_trace_addrop(pmo, rop, file, line, refc, 1);
}

static void
pmo_base_decref(void* pmo_arg, void* rop, char* file, int line)
{
        pmo_base_t* pmo = (pmo_base_t*)pmo_arg;
        int refc;
        
        ASSERT(pmo);
        ASSERT(pmo->refcount > 0);

        refc = atomicAddInt(&pmo->refcount, -1);

        ASSERT(rop);
        ASSERT(file);
        pmo_base_refcd_trace_addrop(pmo, rop, file, line, refc, -1);        

        if (refc == 0) {
                ASSERT(pmo->destroy);
                pmo_destroy(pmo);
                pmo_base_refcd_trace_remove(pmo);
        }
}

void
pmo_base_init(void* pmo_arg, pmo_type_t type, pmo_method_t destructor)
{
        pmo_base_t* pmo = (pmo_base_t*)pmo_arg;
        ASSERT(pmo);
        ASSERT(destructor);

        pmo->incref = (pmo_method_t)pmo_base_incref;
        pmo->decref = (pmo_method_t)pmo_base_decref;
        pmo->destroy = destructor;
        pmo->type = type;
        pmo->refcount = 0;

        /*
         * Add to refcd trace buffer
         */
        pmo_base_refcd_trace_insert(pmo) ;

}

#else /* !MMCI_REFCD */

static void
pmo_base_incref(void* pmo_arg)
{
        pmo_base_t* pmo = (pmo_base_t*)pmo_arg;
        ASSERT(pmo);
  
        atomicAddInt(&pmo->refcount, 1);
}

static void
pmo_base_decref(void* pmo_arg)
{
        pmo_base_t* pmo = (pmo_base_t*)pmo_arg;
        ASSERT(pmo);
        
        ASSERT(pmo->refcount > 0);

        if (atomicAddInt(&pmo->refcount, -1) == 0) {
                ASSERT(pmo->destroy);
                pmo_destroy(pmo);
        }
}

void
pmo_base_init(void* pmo_arg, pmo_type_t type, pmo_method_t destructor)
{
        pmo_base_t* pmo = (pmo_base_t*)pmo_arg;
        ASSERT(pmo);
        ASSERT(destructor);

        pmo->incref = (pmo_method_t)pmo_base_incref;
        pmo->decref = (pmo_method_t)pmo_base_decref;
        pmo->destroy = destructor;
        pmo->type = type;
        pmo->refcount = 0;

}

#endif  /* !MMCI_REFCD */


        





