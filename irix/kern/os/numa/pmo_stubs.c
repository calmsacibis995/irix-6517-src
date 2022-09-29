/*
 * os/numa/pmo_stubs.c
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
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/sysmacros.h>
#include <sys/cpumask.h>
#include <sys/pmo.h>
#include <sys/numa.h>
#include "pmo_base.h"
#include "pmo_error.h"
#include "pmo_list.h"
#include "pmo_ns.h"
#include "mld.h"
#include "raff.h"
#include "mldset.h"
#include "pm.h"
#include "aspm.h"
#include "pmo_process.h"
#include "migr_user.h"
#ifdef CKPT
#include <sys/vnode.h>
#endif

/*
 * MLD stubs
 */

/*ARGSUSED*/
pmo_handle_t
pmo_xface_mld_create(void* inargs, pmo_handle_t required)
{
        return (PMOERR_NOTSUP);
}

/*ARGSUSED*/
pmo_handle_t
pmo_xface_mld_destroy(pmo_handle_t handle)
{
        return (PMOERR_NOTSUP);
}

/*ARGSUSED*/
pmo_handle_t
pmo_xface_mld_getnode(pmo_handle_t handle_arg)
{
	dev_t  node_devt = cnodeid_to_vertex(0);
        return (node_devt);
}

/*
 * MLDSET stubs
 */

/*ARGSUSED*/
pmo_handle_t
pmo_xface_mldset_create(void* inargs, pmo_handle_t required)
{
        return (PMOERR_NOTSUP);
}

/*ARGSUSED*/
int
pmo_xface_mldset_place(void* inargs)
{
        return (PMOERR_NOTSUP);
}

/*ARGSUSED*/
pmo_handle_t
pmo_xface_mldset_destroy(pmo_handle_t mldset_handle)
{
        return (PMOERR_NOTSUP);
}

/*ARGSUSED*/
pmo_handle_t
pmo_xface_process_mldlink(mldlink_info_t* info)
{
        return (PMOERR_NOTSUP);
}

/*ARGSUSED*/
pmo_handle_t
migr_xface_migrate(vrange_t* range, pmo_handle_t handle)
{
        return (PMOERR_NOTSUP);
}

        
#ifdef CKPT
/*ARGSUSED*/
pmo_handle_t
pmo_ckpt_mld_nexthandle(asid_t asid, pmo_handle_t handle, int *rval)
{
	return (pmo_checkerror(PMOERR_ESRCH));
}

/*ARGSUSED*/
pmo_handle_t
pmo_ckpt_mld_info(asid_t asid, pmo_handle_t handle, int *radius, pgno_t *size,
			cnodeid_t *cnodeid, caddr_t *id)
{
	return (pmo_checkerror(PMOERR_ENOENT));
}

/*ARGSUSED*/
void
pmo_ckpt_mldlink_info(mld_t *mld, int *radius, pgno_t *size,
                      cnodeid_t *nodeid, caddr_t *id)
{
}

/*ARGSUSED*/
pmo_handle_t
pmo_ckpt_mldset_info(asid_t asid, pmo_handle_t handle, mldset_info_t *mldinfo, int *rval)
{
	return (pmo_checkerror(PMOERR_ENOENT));
}

/*ARGSUSED*/
pmo_handle_t
pmo_ckpt_mldset_place_info(asid_t asid, mldset_placement_info_t *place, int *rval)
{
	return (pmo_checkerror(PMOERR_ENOENT));
}

/*ARGSUSED*/
pmo_handle_t
pmo_ckpt_mldset_place_ckptinfo(asid_t asid, pmo_handle_t handle, int element,
                               vnode_t **vpp, int *ckpt)
{
	return (pmo_checkerror(PMOERR_EINVAL));
}
#endif
