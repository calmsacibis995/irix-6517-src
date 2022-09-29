/*
 * os/numa/raff.h
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

#ifndef __NUMA_RAFF_H__
#define __NUMA_RAFF_H__

#include <sys/nodemask.h>
/*
 * Resource affinity
 */
struct vnode;

typedef struct raff {
        /*
         * Pmo base
         */
        pmo_base_t base;      /* ref counting */
        /*
         * raff data
         */
        cnodeid_t   cnodeid;    /* Compact nodeid becoming the `pole' (or `zero') */
        cnodemask_t affbv;      /* Other nodes that may have some affinity */
        ushort      radius;     /* Pull (or push) radius */
        ushort      attr;       /* Attractive or Repulsive affinity */
#ifdef CKPT
	struct vnode *vp;	/* to reconstruct path, if physically placed */
	int	    ckpt;
#endif
} raff_t;

#define raff_getnodeid(r)     ((r)->cnodeid)
#define raff_setnodeid(r, n)  ((r)->cnodeid = (n))
#define raff_validnode(r)     (((r)->cnodeid >= 0) && ((r)->cnodeid < numnodes))

extern raff_t* raff_create(cnodeid_t cnodeid,
                           cnodemask_t affbv,
                           ushort radius,
                           ushort attr,
			   struct vnode *,
			   int);
extern void raff_setup(raff_t* raff,
                       cnodeid_t cnodeid,
                       cnodemask_t affbv,
                       ushort radius,
                       ushort attr);
extern void raff_destroy(raff_t* raff);
extern raff_t* raff_convert(raff_info_t* raff_info, pmo_handle_t* perrcode);

#endif /* __NUMA_RAFF_H__ */
