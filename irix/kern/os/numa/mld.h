/*
 * os/numa/mld.h
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

#ifndef __NUMA_MLD_H__
#define __NUMA_MLD_H__

/*
 * Internal structures
 */

typedef struct mld {
        /*
         * Pmo base
         */
        pmo_base_t base;        /* ref counting */
        /*
         * mld data
         */
        cnodeid_t nodeid;       /* physical node where this mld has been placed */
        ushort    radius;       /* locality radius for this mld */
        pgno_t    esize;        /* expected size in pages */
        uint      flags;        /* state flags */
} mld_t;

#if     defined(SV2_MSP)
#define THREADS_PER_MLD         (CPUS_PER_NODE / NSSP_PER_MSP)
#else   /* defined(SV2_MSP) */
#define THREADS_PER_MLD         (CPUS_PER_NODE)
#endif  /* defined(SV2_MSP) */

#define MLD_STATE_PLACED    0x00000001
#define MLD_STATE_PWEAK     0x00000002

#define mld_setnodeid(m, n) ((m)->nodeid = (n))
#define _mld_getnodeid(m)   ((m)->nodeid)
#define	mld_refcount(m)     (pmo_getref(&(m)->base))

#define mld_getradius(m)    ((m)->radius)

#define mld_getsize(m)      ((m)->esize)
#define mld_setsize(m, n)   ((m)->esize = (n))

#define _mld_isplaced(m)     (((m)->flags & MLD_STATE_PLACED) && \
                             ((m)->nodeid >= 0) && \
                             ((m)->nodeid < numnodes))

#define mld_setplaced(m)   (atomicSetInt((int*)&((m)->flags), MLD_STATE_PLACED))
#define mld_setunplaced(m)  (atomicClearInt((int*)&((m)->flags), MLD_STATE_PLACED))

#define mld_setweak(m)      (atomicSetInt((int*)&((m)->flags), MLD_STATE_PWEAK))
#define mld_setunweak(m)    (atomicClearInt((int*)&((m)->flags), MLD_STATE_PWEAK))
#define mld_isweak(m)       ((m)->flags & MLD_STATE_PWEAK)

#define mld_to_bv(m)        (CNODEMASK_CVTB((m)->nodeid))

extern mld_t* mld_create(int radius, pgno_t size);
extern pmo_handle_t mld_create_and_export(int radius,
                                          size_t size,
                                          pmo_ns_t* pmo_ns,
                                          mld_t** ppmld,
					  pmo_handle_t required);
extern void mld_destroy(mld_t* mld);

extern void mld_relocate(mld_t* mld, cnodeid_t dest_node);


extern pmo_handle_t pmo_xface_mld_create(void* inargs, pmo_handle_t required);
extern pmo_handle_t pmo_xface_mld_destroy(pmo_handle_t handle);
extern pmo_handle_t pmo_xface_mld_getnode(pmo_handle_t handle);

#ifdef CKPT
extern pmo_handle_t pmo_ckpt_mld_info(asid_t, pmo_handle_t, int *, pgno_t *,
					cnodeid_t *, caddr_t *);
extern void pmo_ckpt_mldlink_info(mld_t *, int *, pgno_t *, cnodeid_t *, caddr_t *);
#endif
#define pmo_kernel_mld_create(w, x, y) \
        mld_create_and_export((w), (x), (y))


#endif /* __NUMA_MLD_H__ */
