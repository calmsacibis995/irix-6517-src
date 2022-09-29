/*
 * os/numa/mld.c
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
#include <ksys/as.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/pfdat.h>
#include <sys/kabi.h>
#include <sys/sysmacros.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/nodemask.h>
#include <sys/pmo.h>
#include <sys/uthread.h>
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
#include "numa_init.h"
#include "memsched.h"

/*
 * The dynamic memory allocation zone for mld's
 */
static zone_t* mld_zone = 0;

/*
 * The mld initialization procedure.
 * To be called very early during the system
 * initialization procedure.
 */
int
mld_init()
{
        ASSERT(mld_zone == 0);
        mld_zone = kmem_zone_init(sizeof(mld_t), "mld");
        ASSERT(mld_zone);

        return (0);
}


/*
 * An MLD (Memory Locality Domain) is a representation of a
 * physical memory node on a NUMA machine.
 * Every virtual address space gets assigned one or more
 * MLDs, from where the can allocate physical memory.
 */

/**************************************************************************
 *                             MLD Implementation                         *
 **************************************************************************/        

/*
 * This method creates an MLD.
 *
 * + radius
 */
mld_t*
mld_create(int radius, pgno_t size)
{
        mld_t* mld;

        mld = kmem_zone_alloc(mld_zone, KM_SLEEP);
        ASSERT(mld);

        ASSERT(size >= 0);

        /*
         * Catch MLD_DEFAULTSIZE & crazy size values
         */
        ASSERT(size !=  MLD_DEFAULTSIZE);

        mld->nodeid = CNODEID_NONE;
        mld->radius = radius;
        mld->esize  = size;
        mld->flags  = 0;
        
        pmo_base_init(mld, __PMO_MLD, (pmo_method_t)mld_destroy);

        return (mld);
}

pmo_handle_t
mld_create_and_export(int radius,
			size_t size,
			pmo_ns_t* pmo_ns,
			mld_t** ppmld,
			pmo_handle_t required_handle)
{
        mld_t* mld;
        pmo_handle_t handle;
        pgcnt_t esize, rss;

        ASSERT(pmo_ns);


        /*
         * The incoming size is in bytes,
         * and the mld esize field is in *pages*
         */
        
        if (size ==  MLD_DEFAULTSIZE) {
		/* XXX
		 * p0 doesn't have a curuthread, in general it would
		 * be nice to move this up some levels...
		 */
		if (curuthread)
			as_getassize(curuthread->ut_asid, &esize, &rss);
		else
			esize = 0;
        } else {
                esize = btoc(size); /* convert bytes to pages */
        }
        
        /*
         * Create actual mld
         */
        mld = mld_create(radius, esize);
        if (mld == NULL) {
                return (PMOERR_NOMEM);
        }

        /*
         * Export by inserting in pmo_ns
         */
	if (required_handle == (pmo_handle_t)-1) {
        	handle = pmo_ns_insert(pmo_ns, __PMO_MLD, (void*)mld);
	} else {
        	handle = pmo_ns_insert_handle(pmo_ns,
                                              __PMO_MLD,
                                              (void*)mld,
                                              required_handle);
        }
        
        if (pmo_iserrorhandle(handle)) {
                mld_destroy(mld);
                return (handle);
        }

        if (ppmld != NULL) {
                *ppmld = mld;
        }

        return (handle);
}

        
void
mld_destroy(mld_t* mld)
{
        ASSERT (mld);

        /*
         * This is a hook for the `future freemem' management
         * algorithm. We have to give back the memory associated
         * with this mld, if the mld has been placed.
         */

        if (mld->nodeid != CNODEID_NONE) {
                ADD_NODE_FUTURE_FREEMEM_ATOMIC(mld->nodeid, mld->esize);
        }

        kmem_zone_free(mld_zone, mld);
}

void
mld_relocate(mld_t* mld, cnodeid_t dest_node)
{
        physmem_mld_place(mld, dest_node, CNODEMASK_ZERO());
}

int
mld_parbits(mld_t* mld)
{
	return (mld->flags<<16) | mld->nodeid;		/* XXX */
}


/**************************************************************************
 *                            ABI Conversion                              *
 **************************************************************************/

#if _MIPS_SIM == _ABI64
/*ARGSUSED*/
static int
irix5_to_mld_info(
    enum xlate_mode mode,
    void *to,
    int count,
    xlate_info_t *info)
{
    COPYIN_XLATE_PROLOGUE(irix5_mld_info, mld_info);
    target->radius = source->radius;
    target->size = source->size;
    return 0;
}
#endif

/**************************************************************************
 *                                 MLD Xface                              *
 **************************************************************************/

pmo_handle_t
pmo_xface_mld_create(void* inargs, pmo_handle_t required)
{
        mld_info_t mld_info;
        pmo_handle_t handle;

        /*
         * Copy mld_info into kernel space
         */
        if (COPYIN_XLATE(inargs, (void*)&mld_info, sizeof(mld_info_t),
                         irix5_to_mld_info, get_current_abi(), 1)) {
                return (PMOERR_EFAULT);
        }

        numa_message(DM_MLD, 128, "[pmo_xface_mld_create], radius, size",
                     mld_info.radius, mld_info.size);
        
        handle = mld_create_and_export(mld_info.radius,
                                       mld_info.size,
                                       curpmo_ns(),
                                       NULL,
				       required);

        return (handle);
}

pmo_handle_t
pmo_xface_mld_destroy(pmo_handle_t handle)
{
        mld_t* mld;

        mld = (mld_t*)pmo_ns_extract(curpmo_ns(), handle, __PMO_MLD);
        if (mld == NULL) {
                return (PMOERR_INV_MLD_HANDLE);
        }

        /*
         * pmo_ns_extract removes the ref corresponding to the
         * reference from the ns.
         */
        
        return (0);
}

pmo_handle_t
pmo_xface_mld_getnode(pmo_handle_t handle_arg)
{
        mld_t* mld;
	dev_t  node_devt;

        mld = (mld_t *)pmo_ns_find(curpmo_ns(),
                                handle_arg,
                                __PMO_MLD);
        if (mld == NULL) {
                return (PMOERR_INV_PM_HANDLE);
        }

	if (mld_getnodeid(mld) != CNODEID_NONE) {
		node_devt = cnodeid_to_vertex(mld_getnodeid(mld));
        } else {
                node_devt = (dev_t)CNODEID_NONE;
        }
        
        pmo_decref(mld, pmo_ref_find);
	return (node_devt);

}
/**************************************************************************
 *                             Mld Kernel Xface                           *
 **************************************************************************/

/*
 * The kernel constructor for mlds is mld_create_and_export.
 * Aliased via  a macro.
 */

cnodeid_t
mld_getnodeid(mldref_t mldref)
{
        ASSERT(mldref);
        if (_mld_isplaced(mldref) == 0)
		return CNODEID_NONE;
        return (_mld_getnodeid(mldref));
}

int
mld_isplaced(mldref_t mldref)
{
        ASSERT(mldref);
        return (_mld_isplaced(mldref));
}

#ifdef CKPT
/**************************************************************************
 *                          Checkpoint Interfaces                         *
 **************************************************************************/
pmo_handle_t
pmo_ckpt_mld_info(asid_t asid, pmo_handle_t handle, int *radius, pgno_t *size,
		  cnodeid_t *nodeid, caddr_t *id)
{
	pmo_ns_t *ns;
        mld_t *mld;

	ns = getpmo_ns(asid);
	if (ns == NULL)
		return (pmo_checkerror(PMOERR_ENOENT));

        mld = (mld_t*)pmo_ns_find(ns, handle, __PMO_MLD);
        if (mld == NULL) {
		relpmo_ns(asid);
                return (pmo_checkerror(PMOERR_INV_PM_HANDLE));
        }
	*radius = mld_getradius(mld);
	*size = ctob(mld_getsize(mld));
	*nodeid = (mld_isplaced(mld))? mld_getnodeid(mld) : CNODEID_NONE;
	*id = (caddr_t)mld;

        pmo_decref(mld, pmo_ref_find);
	relpmo_ns(asid);

	return (0);
}

void
pmo_ckpt_mldlink_info(mld_t *mld, int *radius, pgno_t *size,
		      cnodeid_t *nodeid, caddr_t *id)
{
	*radius = mld_getradius(mld);
	*size = ctob(mld_getsize(mld));
	*nodeid = (mld_isplaced(mld))? mld_getnodeid(mld) : CNODEID_NONE;
	*id = (caddr_t)mld;
}
#endif
