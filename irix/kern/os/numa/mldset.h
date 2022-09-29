/*
 * os/numa/mldset.h
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

#ifndef __NUMA_MLDSET_H__
#define __NUMA_MLDSET_H__

/***********************************************************
 *                     INTERNAL STRUCTURES                 *
 ***********************************************************/

/*
 * The MLD set
 */
typedef struct mldset {
        /*
         * Pmo base
         */
        pmo_base_t base;                  /* ref counting */
        /*
         * Pmo data - base set
         */
        pmolist_t*      mldlist;          /* list of mlds in set */
        cnodemask_t     mldbv;            /* quick summary of mlds */
        /*
         * Pmo data - placed set
         */
        topology_type_t topology_type;    /* set topology */        
        pmolist_t*      rafflist;         /* list of resources to be close to */
        rqmode_t        rqmode;           /* mandatory or advisory hints */
        mrlock_t        mlock;            /* protect field updates */
} mldset_t;

#define mldset_getmldlist(mldset) ((mldset)->mldlist)
#define mldset_getclen(mldset)    (pmolist_getclen((mldset)->mldlist))
#define mldset_getmldbv(mldset)   ((mldset)->mldbv)
#define mldset_acclock(mldset)    mraccess(&(mldset)->mlock)
#define mldset_updlock(mldset)    mrupdate(&(mldset)->mlock)
#define mldset_accunlock(mldset)  mraccunlock(&(mldset)->mlock)
#define mldset_updunlock(mldset)  mrunlock(&(mldset)->mlock)

#define mldset_getrafflist(mldset) ((mldset)->rafflist)
#define mldset_getrafflen(mldset)  (pmolist_getclen((mldset)->rafflist))
#define mldset_getrqmode(mldset)   ((mldset)->rqmode)

#define MLDSET_GROW_PLACE 0x1

extern mldset_t* mldset_create(pmolist_t* mldlist);
                            
extern pmo_handle_t mldset_create_and_export(pmolist_t* mldlist,
                                             pmo_ns_t* pmo_ns,
                                             mldset_t** ppmldset,
					     pmo_handle_t required);
extern int mldset_place(mldset_t* mldset,
                        topology_type_t topology_type,
                        pmolist_t* rafflist,
                        rqmode_t rqmode);
extern int mldset_grow(mldset_t* mldset, void* pmo, uint flags);
extern void mldset_destroy(mldset_t* mldset);

extern void mldset_relocate(mldset_t* mldset);
extern int mldset_isplaced(mldset_t* mldset);
extern mld_t* mldset_getmld(mldset_t* mldset, int index);

extern pmo_handle_t pmo_xface_mldset_create(void* inargs, pmo_handle_t required);
extern int pmo_xface_mldset_place(void* inargs);
extern pmo_handle_t pmo_xface_mldset_destroy(pmo_handle_t mldset_handle);
extern pmo_handle_t pmo_kernel_mldset_create_export_and_place(topology_type_t topology_type,
                                                              pmolist_t* mldlist,
                                                              pmolist_t* rafflist,
                                                              rqmode_t rqmode,
                                                              pmo_ns_t* pmo_ns,
                                                              mldset_t** ppmldset);

#ifdef CKPT
struct vnode;

extern pmo_handle_t pmo_ckpt_mldset_info(	asid_t asid,
						pmo_handle_t handle,
						mldset_info_t *mldinfo,
						int *rval);
extern pmo_handle_t pmo_ckpt_mldset_place_info(	asid_t asid,
						mldset_placement_info_t *place,
						int *rval);
extern pmo_handle_t pmo_ckpt_mldset_place_ckptinfo(	asid_t asid,
							pmo_handle_t handle,
							int element,
                               				struct vnode **vpp,
							int *ckpt);

#endif

#endif /* __NUMA_MLDSET_H__ */
