/*
 * os/numa/mldset.c
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
#include <sys/kabi.h>
#include <sys/nodepda.h>
#include <sys/sema.h>
#include <sys/pmo.h>
#include "pmo_base.h"
#include "pmo_error.h"
#include "debug_levels.h"
#include "pmo_list.h"
#include "pmo_ns.h"
#include "mld.h"
#include "raff.h"
#include "mldset.h"
#include "pm.h"
#include "aspm.h"
#include "memsched.h"
#include "numa_init.h"
#include "memfit.h"
#ifdef CKPT
#include <sys/vnode.h>
#endif

/*
 * Local methods
 */
static pmolist_t* mldset_mldlist_create(pmo_handle_t* mldlist_info_arg,
                                        int len,
                                        pmo_handle_t* perrcode);
static void mldset_mldlist_destroy(mldset_t* mldset, pmolist_t* mldlist);
static pmolist_t* mldset_rafflist_create(raff_info_t* rafflist_info_arg,
                                         int len,
                                         rqmode_t rqmode,
                                         pmo_handle_t* perrcode);
static void mldset_rafflist_destroy(mldset_t* mldset, pmolist_t* rafflist);


/*
 * The dynamic memory allocation zone for mldset's
 */
static zone_t* mldset_zone = 0;

/*
 * The mldset initialization procedure.
 * To be called very early during the system
 * initialization procedure.
 */
int
mldset_init()
{
        ASSERT(mldset_zone == 0);
        mldset_zone = kmem_zone_init(sizeof(mldset_t), "mldset");
        ASSERT(mldset_zone);

        return (0);
}



/*********************************************************************************
 *                                  MLDSET METHODS                               *
 *********************************************************************************/



mldset_t*
mldset_create(pmolist_t* mldlist)
{
        mldset_t* mldset;

        ASSERT (mldlist);

        mldset = kmem_zone_alloc(mldset_zone, KM_SLEEP);
        ASSERT(mldset);

        /*
         * Set mld list and clear the mldbv field
         * which will be used to keep a quick summary of
         * the nodes where the mlds in this mldset are placed.
         */
        mldset->mldlist = mldlist;
        CNODEMASK_CLRALL(mldset->mldbv);

        /*
         * Initialize placement fields to defaults.
         */
        mldset->topology_type = TOPOLOGY_FREE;
        mldset->rafflist = NULL;
        mldset->rqmode = RQMODE_ADVISORY;

	mrlock_init(&mldset->mlock, MRLOCK_DEFAULT, "mldset", (long)mldset);

        pmo_base_init(mldset, __PMO_MLDSET, (pmo_method_t)mldset_destroy);

        /*
         * To keep the refcount trace sane, update the ref info for the
         * mlds in the mldlist to be referenced by the new mldset.
         */
        pmolist_refupdate(mldlist, mldset);

        return (mldset);
}

pmo_handle_t
mldset_create_and_export(pmolist_t* mldlist,
                         pmo_ns_t* pmo_ns,
                         mldset_t** ppmldset,
			 pmo_handle_t required)
{
        mldset_t* mldset;
        pmo_handle_t handle;

        ASSERT(pmo_ns);
        
        mldset = mldset_create(mldlist);
        ASSERT(mldset != NULL);

	if (required == -1)
        	handle = pmo_ns_insert(pmo_ns, __PMO_MLDSET, (void*)mldset);
	else
		handle = pmo_ns_insert_handle(pmo_ns, __PMO_MLDSET,
						      (void*)mldset, required);
        if (pmo_iserrorhandle(handle)) {
                mldset_destroy(mldset);
                return (handle);
        }

        if (ppmldset != NULL) {
                *ppmldset = mldset;
        }

#ifdef DEBUG_MLDSET_CORRUPTION
        {
                int i;
                printf("[mldset_Create_and_export]:\n");
                printf("mldlist: 0x%x, len: %d\n", mldset->mldlist, mldset->mldlist->clen);\
                for (i = 0; i <  mldset->mldlist->clen; i++) {
                        printf("mld[%d]: 0x%x\n", i, mldset->mldlist->arr[i]);
                }
        }
#endif        
        
        return (handle);
}

int
mldset_place(mldset_t* mldset,
             topology_type_t topology_type,
             pmolist_t* rafflist,
             rqmode_t rqmode)
{
        int v;
        int i;
        
        ASSERT(mldset);
        ASSERT(mldset->mldlist);

        mldset_updlock(mldset);

        mldset->topology_type = topology_type;
        mldset->rafflist = rafflist; /* this field can be NULL */
        mldset->rqmode = rqmode;

        v = physmem_mldset_place(mldset);

        if (v) {
                /*
                 * In case of error, we give up immediately
                 */
                mldset_updunlock(mldset);
                return (v);
        }
        
        /*
         * Construct mldbv which keeps a quick summary of the
         * nodes where the mlds in this mldset have been placed.
         */
        CNODEMASK_CLRALL(mldset->mldbv);
        for (i = 0; i < pmolist_getclen(mldset->mldlist); i++) {
                mld_t* mld = (mld_t*)pmolist_getelem(mldset->mldlist, i);
                ASSERT(mld_isplaced(mld));
                CNODEMASK_SETB(mldset->mldbv, mld_getnodeid(mld));
        }

        /*
         * To keep the refcount trace sane, update the ref info for the
         * raffs in the rafflist to be referenced by the new mldset.
         */
        if (rafflist) {
                pmolist_refupdate(rafflist, mldset);
        }
        
        mldset_updunlock(mldset);

        return (v);
}

void
mldset_destroy(mldset_t* mldset)
{
	pmolist_t	*mlds;
	int		nmlds, i;
	mld_t		*mld;
	extern void	set_loadmask(cnodeid_t);

        ASSERT (mldset);
	
        /*
         * The mld list must always be present
         */
        ASSERT (mldset->mldlist);

	/*
	 * Update the load mask
	 */
	mlds = mldset_getmldlist(mldset);
	nmlds = mldset_getclen(mldset);

	for (i = 0; i < nmlds; i++) {
		mld = (mld_t*)pmolist_getelem(mlds, i);
		if (mld_isplaced(mld)) {
			set_loadmask(mld_getnodeid(mld));
		}
	}

	mldset_mldlist_destroy(mldset, mldset->mldlist);

        /*
         * The rafflist is optional, so we have to
         * check whether we have one.
         */
        if (mldset->rafflist) {
                mldset_rafflist_destroy(mldset, mldset->rafflist);
        }

	mrfree(&mldset->mlock);
        
        kmem_zone_free(mldset_zone, mldset);
}

int
mldset_grow(mldset_t* mldset, void* pmo, uint flags)
{
        pmolist_t* mldlist;
        
        ASSERT(mldset);
        ASSERT(pmo);

        mldset_updlock(mldset);
        
        mldlist = mldset->mldlist;
        ASSERT(mldlist);
        ASSERT(pmolist_getclen(mldlist) > 1);
        
        if (pmo_gettype(pmo) == __PMO_MLD) {
                mld_t* mld = (mld_t*)pmo;
                if (flags & MLDSET_GROW_PLACE) {
                        mld_t* last_mld = (mld_t*)pmolist_getlastelem(mldlist);
                        ASSERT(last_mld);
                        physmem_mld_place(mld,
                                          mld_getnodeid(last_mld),
                                          mldset->mldbv);
                }
                ASSERT(mld_isplaced(mld));

                ASSERT(pmolist_getclen(mldlist) <= pmolist_getmlen(mldlist));

                if (pmolist_getclen(mldlist) == pmolist_getmlen(mldlist)) {
                        pmolist_t* new_mldlist;
                        new_mldlist = pmolist_createfrom(mldlist,
                                                         pmolist_getmlen(mldlist) * 2,
                                                         mldset);
                        if (new_mldlist == NULL) {
                                mldset_updunlock(mldset);
                                return (PMOERR_NOMEM);
                        }
                        mldset->mldlist = new_mldlist;
                        pmolist_destroy(mldlist, mldset);
                }
                
                pmolist_insert(mldset->mldlist, mld, mldset);
                CNODEMASK_SETB(mldset->mldbv, mld_getnodeid(mld));
                
        } else {
                /*
                 * Not implemented yet
                 */
                panic("Not implemented yet (mldset_grow)");
        }

        mldset_updunlock(mldset);

        return (0);
}

void
mldset_relocate(mldset_t* mldset)
{
        ASSERT(mldset);
        
        /*
         * We're embedding some policy here.
         * The idea is that we should relocate an mldset
         * when the scheduler decides to move a process
         * or job to a different section of the machine,
         * permanently. However, this relocation should
         * not happen if the mldset has a raff.
         */
        mldset_updlock(mldset);
        
        if (mldset->rafflist != NULL) {
                /*
                 * We just set each mld to be "placed" again,
                 * without changing the current placement.
                 */
                int i;
                int mldlist_len = mldset_getclen(mldset);
                pmolist_t* mldlist = mldset_getmldlist(mldset);
                for (i = 0; i < mldlist_len; i++) {
                        mld_t* mld = (mld_t*)pmolist_getelem(mldlist, i);
                        mld_setplaced(mld);
                }
        } else {
                /*
                 * We ask the physical memory scheduler to place
                 * this mldset again.
                 */
                (void)physmem_mldset_place(mldset);
        }

#ifdef DEBUG
        {
                int i;
                int mldlist_len = mldset_getclen(mldset);
                pmolist_t* mldlist = mldset_getmldlist(mldset);
                for (i = 0; i < mldlist_len; i++) {
                        mld_t* mld = (mld_t*)pmolist_getelem(mldlist, i);
                        if (!mld_isplaced(mld)) {
                                cmn_err(CE_PANIC,
                                        "(mldset_relocate)UNPLACED MLD, nodeid=%d\n",
                                        mld_getnodeid(mld));      
                        }
                }
        }
#endif /* DEBUG */        
        
      mldset_updunlock(mldset);
}

int
mldset_isplaced(mldset_t* mldset)
{
        int i;
        int mldlist_len;
        pmolist_t* mldlist;
        
        ASSERT(mldset);
        
        mldset_acclock(mldset);
        mldlist_len = mldset_getclen(mldset);
        mldlist = mldset_getmldlist(mldset);
        for (i = 0; i < mldlist_len; i++) {
                mld_t* mld = (mld_t*)pmolist_getelem(mldlist, i);
                if (!mld_isplaced(mld)) {
                        mldset_accunlock(mldset);
                        return (0);
                }
        }
        mldset_accunlock(mldset);
        return (1);
}


mld_t*
mldset_getmld(mldset_t* mldset, int index)
{
        mld_t* mld;
        pmolist_t* mldlist;
        
        ASSERT(mldset);

        mldset_acclock(mldset);
        mldlist = mldset_getmldlist(mldset);
        ASSERT(mldlist);
        mld = (mld_t*)pmolist_getelem(mldlist, index % mldset_getclen(mldset));
        mldset_accunlock(mldset);

        return (mld);
}

/**************************************************************************
 *                            ABI Conversion                              *
 **************************************************************************/

#if _MIPS_SIM == _ABI64

/*ARGSUSED*/
static int
irix5_to_mldset_info(
        enum xlate_mode mode,
        void *to,
        int count,
        xlate_info_t *info)
{
        COPYIN_XLATE_PROLOGUE(irix5_mldset_info, mldset_info);
        target->mldlist = (pmo_handle_t*)(__psint_t)(int)source->mldlist;
        target->mldlist_len = source->mldlist_len;
        return 0;
}


/*ARGSUSED*/
static int
irix5_to_mldset_placement_info(
        enum xlate_mode mode,
        void *to,
        int count,
        xlate_info_t *info)
{
        COPYIN_XLATE_PROLOGUE(irix5_mldset_placement_info, mldset_placement_info);
        target->mldset_handle =  source->mldset_handle;
        target->topology_type = (topology_type_t)source->topology_type;
        target->rafflist = (raff_info_t*)(__psint_t)(int)source->rafflist;
        target->rafflist_len = source->rafflist_len;
        target->rqmode = (rqmode_t)source->rqmode;
        return 0;
}

/*ARGSUSED*/
static int
irix5_to_raff_info(
        enum xlate_mode mode,
        void *to,
        int count,
        xlate_info_t *info)
{
        int srcsz = count * sizeof(struct irix5_raff_info);

        COPYIN_XLATE_VARYING_PROLOGUE(irix5_raff_info, raff_info, srcsz);

        while (count) {
                target->resource = (void*)(__psint_t)(int)source->resource;
                target->reslen = source->reslen;
                target->restype = source->restype;
                target->radius = source->radius;
                target->attr = source->attr;
                target++;
                source++;
                count--;
        }
        return 0;
}

#endif /* _ABI64 */ 

/**************************************************************************
 *                          Mldset Internal Methods                       *
 **************************************************************************/

static pmolist_t*
mldset_mldlist_create(pmo_handle_t* mldlist_info_arg, int len, pmo_handle_t* perrcode)
{
        pmo_handle_t* mldlist_info;
        pmolist_t* mldlist;
        int i;

        /*
         * Alloc memory for our local mldlist_info
         */
        mldlist_info = kmem_zalloc(len*sizeof(pmo_handle_t), KM_SLEEP);
        ASSERT(mldlist_info);
            
        /*
         * Copy the mldlist_info array into kernel space
         * This is an array of `int', which is the same
         * width in both the 32 and 64 bit worlds.
         */
        if (copyin((void*)mldlist_info_arg,
                   (void*)mldlist_info,
                   len*sizeof(pmo_handle_t))) {
                kmem_free(mldlist_info, len*sizeof(pmo_handle_t));
                *perrcode = PMOERR_EFAULT;
                return (NULL);
        }

        /*
         * Create the real mldlist
         */
        if ((mldlist = pmolist_create(len)) == NULL) {
                kmem_free(mldlist_info, len*sizeof(pmo_handle_t));
                *perrcode = PMOERR_NOMEM;
                return (NULL);
        }                
                
        for (i = 0; i < len; i++) {
                int j;
                mld_t* mld;

                mld = (mld_t*)pmo_ns_find(curpmo_ns(),
                                          mldlist_info[i],
                                          __PMO_MLD);
                if (mld == NULL) {
                        /*
                         * We have to release the refs acq by find
                         */
                        for (j = 0; j < pmolist_getclen(mldlist); j++) {
                                pmo_decref(pmolist_getelem(mldlist, j), pmo_ref_find);
                        }
                        pmolist_destroy(mldlist, pmo_ref_list);
                        kmem_free(mldlist_info, len*sizeof(pmo_handle_t));
                        *perrcode = PMOERR_INV_MLD_HANDLE;
                        return (NULL);
                }
                pmolist_insert(mldlist, mld, pmo_ref_list);
        }

        /*
         * Release the find references
         */
        for (i = 0; i < pmolist_getclen(mldlist); i++) {
                pmo_decref(pmolist_getelem(mldlist, i), pmo_ref_find);
        }        

        /*
         * We don't need the local version of mldlist_info anymore.
         */
        kmem_free(mldlist_info, len*sizeof(pmo_handle_t)); 

        return (mldlist);
}

static void
mldset_mldlist_destroy(mldset_t* mldset, pmolist_t* mldlist)
{
        ASSERT (mldlist);
        
        pmolist_destroy(mldlist, mldset);
}

static pmolist_t*
mldset_rafflist_create(raff_info_t* rafflist_info_arg,
                       int len,
                       rqmode_t rqmode,
                       pmo_handle_t* perrcode)
{
        raff_info_t* rafflist_info;
        pmolist_t* rafflist;
        raff_t* raff;
        int i;

        ASSERT(rafflist_info_arg);
        ASSERT(len);
        ASSERT(perrcode);
        
        /*
         * Allocate memory for the local version of rafflist_info
         */
        rafflist_info = kmem_zalloc(len*sizeof(raff_info_t), KM_SLEEP);
        ASSERT(rafflist_info);
        
        /*
         * Copy the rafflist_info array into kernel space
         */
        if (COPYIN_XLATE((void*)rafflist_info_arg,
                         (void*)rafflist_info,
                         len*sizeof(raff_info_t),
                         irix5_to_raff_info,
                         get_current_abi(), len)) {
                kmem_free(rafflist_info, len * sizeof(raff_info_t));
                *perrcode = PMOERR_EFAULT;
                return (NULL);
        }

        /*
         * Create the rafflist
         */
        if ((rafflist = pmolist_create(len)) == NULL) {
                kmem_free(rafflist_info, len * sizeof(raff_info_t));
                *perrcode = PMOERR_NOMEM;
                return (NULL);
        }
                
        /*
         * Create internal raff descriptors based on the external raff_info's,
         * converting the resource spec into a compact node id.
         */

        for (i = 0; i < len; i++) {
                raff = raff_convert(&rafflist_info[i], perrcode);
                if (raff == NULL) {
                        break;
                } else {
                        if (!raff_validnode(raff)) {
                                if (rqmode == RQMODE_ADVISORY) {
                                        raff_setnodeid(raff, cnodeid());
                                } else {
                                        break;
                                }
                        }
                        ASSERT(raff_validnode(raff));
                        pmolist_insert(rafflist, raff, pmo_ref_list);
                }
        }

        if (i != len) {
                /*
                 * Exception
                 */

#ifdef DEBUG
                int j;
                extern pmo_base_print(pmo_base_t* pmo_base);
                printf("RAFF Exception:\n");

                for (j = 0; j < pmolist_getclen(rafflist); j++) {
                        pmo_base_print((pmo_base_t*)pmolist_getelem(rafflist, j));
                }
#endif /* DEBUG */                        
                kmem_free(rafflist_info, len * sizeof(raff_info_t));
                pmolist_destroy(rafflist, pmo_ref_list);
                *perrcode = PMOERR_EINVAL;
                return (NULL);
        }
        
        /*
         * We don't need the local version of rafflist_info anymore
         */
        kmem_free(rafflist_info, len * sizeof(raff_info_t));

        return (rafflist);
}

static void
mldset_rafflist_destroy(mldset_t* mldset, pmolist_t* rafflist)
{
        ASSERT (rafflist);
        
        pmolist_destroy(rafflist, mldset);  
}


/**************************************************************************
 *                          Mldset User Xface                             *
 **************************************************************************/


pmo_handle_t
pmo_xface_mldset_create(void* inargs, pmo_handle_t required)
{
        mldset_info_t mldset_info;
        pmolist_t* mldlist;
        pmo_handle_t errcode;
        pmo_handle_t handle;

        if (COPYIN_XLATE(inargs, (void*)&mldset_info, sizeof(mldset_info_t),
                         irix5_to_mldset_info, get_current_abi(), 1)) {
                return (PMOERR_EFAULT);
        }

        /*
         * The number of mlds must be >= 1
         */
        if (mldset_info.mldlist_len < 1) {
                return (PMOERR_INV_MLDLIST);
        }

        /*
         * Since the number of mlds must be >= 1,
         * the mldlist pointer must be non-null.
         */
        if (mldset_info.mldlist == NULL) {
                return (PMOERR_EFAULT);
        }

        /*
         * Setup mldlist
         */
        mldlist = mldset_mldlist_create(mldset_info.mldlist,
                                        mldset_info.mldlist_len,
                                        &errcode);
        if (mldlist == NULL) {
                return (errcode);
        }

        handle = mldset_create_and_export(mldlist,
                                          curpmo_ns(),
                                          NULL,
					  required);

        /*
         * In case of error, we don't need to
         * destroy the mldlist or
         * the raff list because they are destroyed
         * in mldset_create_and_export when an error
         * condition is encountered and the whole newly
         * created mldset has to be destroyed.
         */

        return (handle);
}

int
pmo_xface_mldset_place(void* inargs)
{
        mldset_placement_info_t mldset_placement_info;
        mldset_t* mldset;
        pmolist_t* rafflist;
        pmo_handle_t errcode;

        if (COPYIN_XLATE(inargs,
                         (void*)&mldset_placement_info,
                         sizeof(mldset_placement_info_t),
                         irix5_to_mldset_placement_info,
                         get_current_abi(), 1)) {
                return (PMOERR_EFAULT);
        }

        numa_message(DM_MLDSET, 128, "[pmo_xface_mld_create], sethandle, top",
                     mldset_placement_info.mldset_handle,
                     mldset_placement_info.topology_type);
        /*
         * Convert the mldset handle to an mldset reference & take ref.
         */
        mldset = (mldset_t*)pmo_ns_find(curpmo_ns(),
                                        mldset_placement_info.mldset_handle,
                                        __PMO_MLDSET);
        if (mldset == NULL) {
                numa_message(DM_MLDSET, 128, "OUT on invalid mldset handle",
			     0, 0);
                return (PMOERR_INV_MLDSET_HANDLE);
        }

        /*
         * At this point, we've verified the handle was valid,
         * and we have a reference on the mldset.
         */
        
        /*
         * Verify validity of  arguments
         */

        /*
         * The topology spec must be one of the options
         * in topology_type_t.
         */
        if (mldset_placement_info.topology_type < 0 ||
            mldset_placement_info.topology_type >= TOPOLOGY_LAST) {
                errcode = PMOERR_INV_TOPOLOGY;
                goto out;
        }

        /*
         * If the number of raffs is NULL, then the
         * length of the raff list must be 0, and
         * viceversa.
         */
        if (mldset_placement_info.rafflist == NULL) {
                if (mldset_placement_info.rafflist_len != 0) {
                       errcode = PMOERR_INV_RAFFLIST;
                       goto out;
                }
        }

        if (mldset_placement_info.rafflist_len == 0) {
                if (mldset_placement_info.rafflist != NULL) {
                        errcode = PMOERR_INV_RAFFLIST;
                        goto out;
                }
        }

        /*
         * The hint mode must be a valid flag.
         * RAFFATTR contains all the bits that maybe set.
         */
        if (mldset_placement_info.rqmode & ~RAFFATTR_VALID) {
                errcode = PMOERR_INV_RQMODE;
                goto out;
        }

        /*
         * Setup rafflist
         */
        if (mldset_placement_info.rafflist != NULL) {
                rafflist = mldset_rafflist_create(mldset_placement_info.rafflist,
                                                  mldset_placement_info.rafflist_len,
                                                  mldset_placement_info.rqmode,
                                                  &errcode);
                if (rafflist == NULL) {
                        goto out;
                }
        } else {
                rafflist = NULL;
        }

        
 
        errcode =  mldset_place(mldset,
                                mldset_placement_info.topology_type,
                                rafflist,
                                mldset_placement_info.rqmode);

#ifdef DEBUG        
        if ((errcode == 0) && rafflist) {
                int i;
                pmolist_t* mldlist = mldset_getmldlist(mldset);
                numa_message(DM_MLDSET, 128, "MLDSET Placement\n", 0, 0);
                numa_message(DM_MLDSET, 128, "Raff List:\n", 0, 0);
                for (i = 0; i < pmolist_getclen(rafflist); i++) {
			/*REFERENCED*/
                        raff_t* raff = (raff_t*)pmolist_getelem(rafflist, i);
                        numa_message(DM_MLDSET, 128, "RAFF[%d] on node %d\n", 
				     i, raff_getnodeid(raff));
                }
                for (i = 0; i < pmolist_getclen(mldlist); i++) {
			/*REFERENCED*/
                        mld_t* mld = (mld_t*)pmolist_getelem(mldlist, i);
                        numa_message(DM_MLDSET, 128, "MLD[%d] on node %d\n", 
				     i, mld_getnodeid(mld));
                }
        }
#endif /* DEBUG */
        
  out:
        numa_message(DM_MLDSET, 128, "GOING OUT with code: %d\n", errcode, 0);
        pmo_decref(mldset, pmo_ref_find);
        return (errcode);
}

pmo_handle_t
pmo_xface_mldset_destroy(pmo_handle_t mldset_handle)
{
        mldset_t* mldset;

        mldset = (mldset_t*)pmo_ns_extract(curpmo_ns(), mldset_handle, __PMO_MLDSET);
        if (mldset == NULL) {
                return (PMOERR_INV_MLDSET_HANDLE);
        }

        /*
         * pmo_ns_extract removes the ref corresponding to the
         * reference from the ns.
         */
        

        return (0);
}

/**************************************************************************
 *                          Mldset Kernel Xface                           *
 **************************************************************************/

pmo_handle_t
pmo_kernel_mldset_create_export_and_place(topology_type_t topology_type,
                                          pmolist_t* mldlist,
                                          pmolist_t* rafflist,
                                          rqmode_t rqmode,
                                          pmo_ns_t* pmo_ns,
                                          mldset_t** ppmldset)
{
        pmo_handle_t handle;
        pmo_handle_t errhandle;

        ASSERT(mldlist);
        ASSERT(pmo_ns);
        ASSERT(ppmldset);
        ASSERT(pmo_ns);

         handle = mldset_create_and_export(mldlist,
                                          pmo_ns,
                                          ppmldset,
					  -1);

        if (pmo_iserrorhandle(handle)) {
                return (handle);
        }

        errhandle =  mldset_place(*ppmldset,
                                  topology_type,
                                  rafflist,
                                  rqmode);
        
        if (pmo_iserrorhandle(errhandle)) {
                mldset_destroy(*ppmldset);
                return (errhandle);
        }
        
        return (handle);
}
#ifdef CKPT
/**************************************************************************
 *                          Checkpoint Interfaces                         *
 **************************************************************************/
pmo_handle_t
pmo_ckpt_mldset_info(asid_t asid, pmo_handle_t handle, mldset_info_t *mldinfo, int *rval)
{
	pmo_ns_t *ns;
        mldset_t *mldset;
	int i;
	mld_t *mld;
	pmo_handle_t mldhandle;

	ns = getpmo_ns(asid);
	if (ns == NULL)
		return (pmo_checkerror(PMOERR_ENOENT));

        mldset = (mldset_t*)pmo_ns_find(ns, handle, __PMO_MLDSET);
        if (mldset == NULL) {
		relpmo_ns(asid);
                return (pmo_checkerror(PMOERR_INV_PM_HANDLE));
        }
	for (i = 0; i < min(mldinfo->mldlist_len, mldset_getclen(mldset)); i++) {
		mld = mldset_getmld(mldset, i);
		mldhandle = pmo_ns_pmo_lookup(ns, __PMO_MLD, mld);

		if (pmo_iserrorhandle(mldhandle)) {
        		pmo_decref(mldset, pmo_ref_find);
			relpmo_ns(asid);
			return (pmo_checkerror(mldhandle));
		}
		if (copyout(&mldhandle, mldinfo->mldlist++, sizeof(pmo_handle_t))) {
        		pmo_decref(mldset, pmo_ref_find);
			relpmo_ns(asid);
			return (pmo_checkerror(PMOERR_EFAULT));
		}
	}
        pmo_decref(mldset, pmo_ref_find);
	relpmo_ns(asid);

	*rval = mldset_getclen(mldset);

	return (0);
}

pmo_handle_t
pmo_ckpt_mldset_place_info(asid_t asid, mldset_placement_info_t *place, int *rval)
{
	pmo_ns_t *ns;
        mldset_t *mldset;
	pmolist_t *rafflist;
	raff_t *raff;
	raff_info_t *raff_ptr = place->rafflist;
	raff_info_t raff_info;
	int i;

	ns = getpmo_ns(asid);
	if (ns == NULL)
		return (pmo_checkerror(PMOERR_ENOENT));

        mldset = (mldset_t*)pmo_ns_find(ns, place->mldset_handle, __PMO_MLDSET);
        if (mldset == NULL) {
		relpmo_ns(asid);
                return (pmo_checkerror(PMOERR_INV_PM_HANDLE));
        }
	if (!mldset_isplaced(mldset)) {
		place->mldset_handle = -1;
		*rval = 0;
        	pmo_decref(mldset, pmo_ref_find);
		relpmo_ns(asid);
		return (0);
	}
	place->topology_type = mldset->topology_type;
	place->rqmode = mldset->rqmode;

	rafflist = mldset->rafflist;

	if (rafflist == NULL) {
		*rval = 0;
        	pmo_decref(mldset, pmo_ref_find);
		relpmo_ns(asid);
		return (0);
	}
	for (i = 0; i < min(place->rafflist_len,pmolist_getclen(rafflist)); i++) {
		raff = (raff_t *)pmolist_getelem(rafflist, i);

		raff_info.radius = raff->radius;
		raff_info.attr = raff->attr;

		if (copyout(&raff_info, raff_ptr++, sizeof(raff_info))) {
        		pmo_decref(mldset, pmo_ref_find);
			relpmo_ns(asid);
			return (pmo_checkerror(PMOERR_EFAULT));
		}
	}
        pmo_decref(mldset, pmo_ref_find);
	relpmo_ns(asid);

	*rval = pmolist_getclen(mldset->rafflist);

	return (0);
}

pmo_handle_t
pmo_ckpt_mldset_place_ckptinfo(asid_t asid, pmo_handle_t handle, int element,
			       vnode_t **vpp, int *ckpt)
{
	pmo_ns_t *ns;
        mldset_t *mldset;
	pmolist_t *rafflist;
	raff_t *raff;

	ns = getpmo_ns(asid);
	if (ns == NULL)
		return (pmo_checkerror(PMOERR_ENOENT));

        mldset = (mldset_t*)pmo_ns_find(ns, handle, __PMO_MLDSET);
        if (mldset == NULL) {
		relpmo_ns(asid);
                return (pmo_checkerror(PMOERR_INV_PM_HANDLE));
        }
	if (!mldset_isplaced(mldset)) {
        	pmo_decref(mldset, pmo_ref_find);
		relpmo_ns(asid);
                return (pmo_checkerror(PMOERR_EINVAL));
	}
	rafflist = mldset->rafflist;

	if (rafflist == NULL) {
        	pmo_decref(mldset, pmo_ref_find);
		relpmo_ns(asid);
                return (pmo_checkerror(PMOERR_EINVAL));
	}
	if ((element < 0)||(element >= pmolist_getclen(rafflist))) {
        	pmo_decref(mldset, pmo_ref_find);
		relpmo_ns(asid);
                return (pmo_checkerror(PMOERR_EINVAL));
	}
	raff = (raff_t *)pmolist_getelem(rafflist, element);

	ASSERT(raff);

	if (raff->vp)
		VN_HOLD(raff->vp);

	*vpp = raff->vp;
	*ckpt = raff->ckpt;

        pmo_decref(mldset, pmo_ref_find);
	relpmo_ns(asid);

	return (0);
}
#endif
