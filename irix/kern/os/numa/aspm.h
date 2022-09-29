/*
 * os/numa/aspm.h
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

#ifndef __NUMA_ASPM_H__
#define __NUMA_ASPM_H_

/*
 * The maximum number of entries in the pmo_ns
 * table where we keep handles to all the objects
 * we've created for policy management that are
 * visible to users.
 * This table grows automatically when filled up.
 */
#ifdef MP
#define PMO_NS_MAXENTRIES      64
#else
#define PMO_NS_MAXENTRIES      8
#endif

/*
 * This is the base object linked to the proc structure for
 * Memory Management Policies.
 *
 * + pmo_ns:
 *   a name space where we place handles
 *   visible to users for all the policy
 *   management objects created on behalf of
 *   this address space.
 *
 * + pm_default_ref:
 *   a direct reference to the default policy module
 *   for this address space.
 *
 * + pm_default_handle:
 *   the handle associated with the default policy module.
 *   We keep it here in order to have it easily available
 *   and identifiable when users ask for the default handle.
 */

typedef struct aspm {
        /*
         * Policy
         */
        pmo_ns_t*    pmo_ns;           /* per as pmo table */
        pm_t*        pm_default_ref[NUM_MEM_TYPES];   /* default pmgroup ref */
        pmo_handle_t pm_default_hdl[NUM_MEM_TYPES]; /* default pmgroup handle */
        lock_t       lock;         /* protect updates of pm_default[ref|hdl] */
} aspm_t;

/*
 * Get the pmo_ns for the address space associated to the
 * current context.
 */
extern pmo_ns_t* curpmo_ns(void);
extern pmo_ns_t* getpmo_ns(asid_t);
extern int relpmo_ns(asid_t);

/*
 * return aspm for arbitrary AS
 * NOTE: this takes a reference on the aspm, be careful to drop it!
 */
extern aspm_t *getaspm(asid_t);

/*
 * aspm locking
 */
#define aspm_lock(aspm)      mutex_spinlock(&(aspm)->lock)
#define aspm_unlock(aspm, s) mutex_spinunlock(&(aspm)->lock, (s))


/*
 * Create new affinity link and set
 */
#define aspm_proc_affset(kt, mld, affbv)                \
	{						\
		int s; 					\
		ASSERT((kt));                           \
		ASSERT((mld));                          \
		s = kt_lock((kt));                                        \
		pmo_incref((mld), (kt));                                  \
		if ((kt)->k_mldlink) {                                    \
		     ASSERT(pmo_gettype((kt)->k_mldlink) == __PMO_MLD);   \
		     pmo_decref((kt)->k_mldlink, (kt));                   \
		}                                                         \
		(kt)->k_mldlink = (mld);                                  \
		if ((mld))					  \
			(kt)->k_affnode = mld_getnodeid((mld));		  \
		else							  \
			(kt)->k_affnode = CNODEID_NONE;			  \
		(kt)->k_maffbv = (affbv);                                 \
		kt_unlock((kt), s);                                       \
	}

                       
/*
 * Local public methods
 * (Global public methods are declared in sys/numa.h)
 */

extern pmo_handle_t pmo_xface_aspm_getdefaultpm(mem_type_t);
extern pmo_handle_t pmo_xface_aspm_setdefaultpm(pmo_handle_t newpm_handle, mem_type_t);
extern void aspm_relocate(aspm_t* aspm, cnodeid_t dest_node);

extern cnodemask_t pmo_xface_aspm_getcnodemask(void);
extern cnodemask_t pmo_xface_aspm_setcnodemask(cnodemask_t);

#endif /* __NUMA_ASPM_H_ */
