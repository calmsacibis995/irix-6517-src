/*
 * os/numa/pmfactory_ns.h
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

#ifndef __NUMA_PMFACTORY_H__
#define __NUMA_PMFACTORY_H__

/**************************************************************************
 *                         PM  Factory Naming                             *
 **************************************************************************/

/*
 * An entry in the policy module factory table
 */
typedef struct pmfactory_ns_entry {
        char           pm_name[PM_NAME_SIZE];
        uint           pm_type;
        pmfactory_t    pmfactory;
} pmfactory_ns_entry_t;

/*
 * The policy module factory table
 */
typedef struct pmfactory_ns {
        pmfactory_ns_entry_t pmfactory_ns_entry[PMFACTORY_NS_NENT];
        int index;           /* store next */
        int nentries;        /* current valid entries */
        lock_t lock;
} pmfactory_ns_t;

/*
 * Interator over the factory table
 */
typedef struct pmfactory_ns_it {
        pmfactory_ns_t* pmfactory_ns;  /* ns we're iterating over */
        int it_index;                  /* next element to return */
} pmfactory_ns_it_t;


/*
 * Locking
 */

#define pmfactory_ns_lock(ns)       mutex_spinlock(&(ns)->lock)
#define pmfactory_ns_unlock(ns, s)  mutex_spinunlock(&(ns)->lock, (s))



/*
 * Methods
 */

extern pmfactory_ns_t* pmfactory_ns_create(void);
extern pmo_handle_t pmfactory_ns_insert(pmfactory_ns_t* pmfactory_ns,
                                       char* pm_name,
                                       pmfactory_t pmfactory);
extern pmfactory_t pmfactory_ns_find(pmfactory_ns_t* pmfactory_ns,
                                      char* pm_name);
extern pmfactory_t pmfactory_ns_extract(pmfactory_ns_t* pmfactory_ns,
                                         char* pm_name);
extern char* pmfactory_ns_entry_format(pmfactory_ns_entry_t* entry, char* str, int len);
extern pmfactory_ns_it_t* pmfactory_ns_it_create(pmfactory_ns_t* pmfactory_ns);
extern void pmfactory_ns_it_destroy(pmfactory_ns_it_t* pmfactory_ns_it);
extern pmfactory_ns_entry_t* pmfactory_ns_it_getnext(pmfactory_ns_it_t* pmfactory_ns_it);

extern pmfactory_ns_t* pmfactory_ns_getdefault(void);

#endif /* __NUMA_PMFACTORY_H__ */
