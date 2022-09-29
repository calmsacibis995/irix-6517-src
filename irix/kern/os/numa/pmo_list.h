/*
 * os/numa/pmo_list.h
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

#ifndef __NUMA_PMO_LIST_H__
#define __NUMA_PMO_LIST_H__


/*
 * XXX TBD:
 * allocate a bit of static memory for the list.
 * most of the time the list will contain only a few mlds
 */
 
/*
 * A fixed list of pmos
 */
typedef struct pmolist {
        int      mlen;      /* max len (allocated memory */
        int      clen;      /* current len (index to next avail slot) */
        void**   arr;       /* array of pmos, or single pmo when len == 1 */
} pmolist_t;

#define pmolist_getclen(l)     ((l)->clen)
#define pmolist_getmlen(l)     ((l)->mlen)
#define pmolist_getlastelem(l) (pmolist_getelem((l), (l)->clen - 1))

extern pmolist_t* pmolist_create(int len);
extern void pmolist_destroy(pmolist_t* pmolist, void* rop);
extern void pmolist_setup(pmolist_t* pmolist, void* pmo);
extern void pmolist_insert(pmolist_t* pmolist, void* pmo, void* rop);
extern void* pmolist_getelem(pmolist_t* pmolist, int i);
extern pmolist_t* pmolist_createfrom(pmolist_t* srclist, int newlen, void* rop);
extern void pmolist_print(pmolist_t* pmolist);
extern void pmolist_refupdate(pmolist_t* pmolist, void* pmo);

#endif /* __NUMA_PMO_LIST_H__ */
