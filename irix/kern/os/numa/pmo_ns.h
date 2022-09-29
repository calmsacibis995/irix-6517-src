/*
 * os/numa/pmo_ns.h
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


#ifndef __NUMA_PMO_NS_H__
#define __NUMA_PMO_NS_H__

/**************************************************************************
 *                Naming Table for PM's and MLD's  (PMO's)                *
 **************************************************************************/


/*
 * An entry in the pmo table
 */
typedef struct pmo_ns_entry {
        pmo_handle_t   pmo_handle;
        pmo_type_t     pmo_type;
        void*          pmo;
} pmo_ns_entry_t;

/*
 * The Policy Management Object Table
 * For now this is just a linear table,
 * but we may need to use a more space efficient
 * approach if the number of entries grows beyond a few tens.
 */
typedef struct pmo_ns {
	pmo_base_t	base;             
        pmo_ns_entry_t* pmo_ns_entry;
        int             maxentries;
        int             nentries;
        int             index;
        int             flags;
        lock_t          lock;
        sv_t            wait;
} pmo_ns_t;

/*
 * pmo_ns flags
 */
#define PMO_NS_EXPANDING   0x0001
#define PMO_NS_WAITING     0x0002
#define PMO_NS_DESTROYING  0x0004

/*
 * A pmo table iterator
 */
typedef struct pmo_ns_it {
        pmo_ns_t* pmo_ns;
        int       it_index;
} pmo_ns_it_t;


/*
 * Locking
 */
#define pmo_ns_lock(ns)        mutex_spinlock(&(ns)->lock)
#define pmo_ns_unlock(ns, s)   mutex_spinunlock(&(ns)->lock, (s))


#define PMO_NS_BUILDHANDLE(ix)  ((ix))
#define PMO_NS_GETINDEX(hd)     ((hd))

/*
 * Methods
 */

extern pmo_ns_t* pmo_ns_create(int maxentries);
extern void pmo_ns_destroy(pmo_ns_t* pmo_ns);
extern pmo_handle_t pmo_ns_insert(pmo_ns_t* pmo_ns, pmo_type_t pmo_type, void* pmo);
extern pmo_handle_t pmo_ns_insert_handle(pmo_ns_t* pmo_ns,
					pmo_type_t pmo_type,
					void* pmo,
			  		pmo_handle_t pmo_handle);
extern void* pmo_ns_find(pmo_ns_t* pmo_ns,
                          pmo_handle_t pmo_handle,
                          pmo_type_t pmo_type);
extern pmo_handle_t pmo_ns_find_handle(pmo_ns_t* pmo_ns,
                          pmo_handle_t pmo_handle,
                          pmo_type_t pmo_type);
extern pmo_handle_t pmo_ns_pmo_lookup(pmo_ns_t* pmo_ns, pmo_type_t pmo_type, void* pmo);
extern void* pmo_ns_extract(pmo_ns_t* pmo_ns,
                             pmo_handle_t pmo_handle,
                             pmo_type_t pmo_type);
extern char* pmo_ns_entry_format(pmo_ns_entry_t* entry, char* str, int len);
extern void pmo_ns_it_init(pmo_ns_it_t* pmo_ns_it, pmo_ns_t* pmo_ns);
extern pmo_ns_entry_t* pmo_ns_it_getnext(pmo_ns_it_t* pmo_ns_it);

#endif /* __NUMA_PMO_NS_H__ */
