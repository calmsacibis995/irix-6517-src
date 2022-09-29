/*
 * os/numa/raff.c
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
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <sys/cpumask.h>
#include <sys/pmo.h>
#include <sys/numa.h>
#include <sys/vnode.h>
#include <sys/hwgraph.h>
#include <sys/nodemask.h>
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

/*
 * RAFFs (Resource Affinity descriptors) define the `poles'
 * of attraction for a memory space (or the zeros of repulsion).
 * These poles may be specified by users to explicitely
 * request proximity to resources, or internally by
 * the kernel to gravitate processes towards the nodes
 * containing their vnode-data.
 */

/*
 * The dynamic memory allocation zone for mld's
 */
static zone_t* raff_zone = 0;

/*
 * The raff initialization procedure.
 * To be called very early during the system
 * initialization procedure.
 */
int
raff_init()
{
        ASSERT(raff_zone == 0);
        raff_zone = kmem_zone_init(sizeof(raff_t), "raff");
        ASSERT(raff_zone);

        return (0);
}

/*****************************************************************
 *                        Kernel Interface                       *
 *****************************************************************/
/* ARGSUSED */
raff_t*
raff_create(cnodeid_t cnodeid, cnodemask_t affbv, ushort radius, ushort attr,
						vnode_t *vp, int ckpt)
{
        raff_t* raff;

        raff = kmem_zone_alloc(raff_zone, KM_SLEEP);
        ASSERT(raff);

        raff->cnodeid = cnodeid;
        raff->affbv = affbv;
        raff->radius = radius;
        raff->attr = attr;
#ifdef CKPT
	raff->vp = vp;
	raff->ckpt = ckpt;
#endif
        /*
         * If we start reference counting on raffs, then
         * we need to change the destructor accordingly.
         */
        pmo_base_init(raff, __PMO_RAFF, (pmo_method_t)raff_destroy);

        return (raff);
}

void
raff_setup(raff_t* raff,
           cnodeid_t cnodeid,
           cnodemask_t affbv,
           ushort radius,
           ushort attr)
{
        ASSERT(raff);
        raff->cnodeid = cnodeid;
        raff->affbv = affbv;
        raff->radius = radius;
        raff->attr = attr;
}


void
raff_destroy(raff_t* raff)
{
        ASSERT (raff);
#ifdef CKPT
	if (raff->vp)
		VN_RELE(raff->vp);
#endif
        kmem_zone_free(raff_zone, raff);
}


/*
 * Convert a user-level raff into a kernel-level raff.
 * `name' is the hardware graph path of the resource
 * we want affinity to. The name may be a node name.
 */

raff_t*
raff_convert(raff_info_t* raff_info, pmo_handle_t* perrcode)
{
        cnodeid_t cnodeid;
        raff_t* raff;
        vnode_t* vp = NULL;
	int ckpt = -1;
#ifdef CKPT
	ckpt_handle_t lookup = NULL;
#endif

        ASSERT(raff_info);
        ASSERT(perrcode);

        switch (raff_info->restype) {
        case RAFFIDT_NAME:
                if(raff_info->resource == NULL) {
                        *perrcode = PMOERR_EFAULT;
                        return (NULL);
                }
                
                numa_message(DM_MLDSET, 128,  "[raff_convert]: len, type",
                             raff_info->reslen, raff_info->restype);
                numa_message(DM_MLDSET, 128, raff_info->resource, 0, 0);
                numa_message(DM_MLDSET, 128, "[raff_convert]: radius, affinity",
                             raff_info->radius, raff_info->attr);

                if (lookupname(raff_info->resource,
			       UIO_USERSPACE,
			       FOLLOW,
			       NULLVPP,
			       &vp,
#ifdef CKPT
			       &lookup)) {
#else
			       NULL)) {
#endif
                        *perrcode = PMOERR_ENOENT;
                        numa_message(DM_RAFF, 128, "Lookup FAILED\n", 0, 0);
                        return (NULL);
                }
                cnodeid = master_node_get(vp->v_rdev);
#ifdef CKPT
		ASSERT(vp);

		if (lookup)
			ckpt = ckpt_lookup_add(vp, lookup);
#else
                VN_RELE(vp);
#endif
                numa_message(DM_RAFF, 128, "[raff_convert], Device on Node:",
                             cnodeid, 0);

                break;

        case RAFFIDT_FD:
                *perrcode = PMOERR_EINVAL;
                return (NULL);

        default:
               *perrcode = PMOERR_EINVAL;
               return (NULL);
        }

        if ((raff = raff_create(cnodeid,
                                CNODEMASK_ZERO(),
                                raff_info->radius,
                                raff_info->attr,
				vp,
				ckpt)) == NULL) {
#ifdef CKPT
		ASSERT(vp);
		VN_RELE(vp);
#endif
                *perrcode = PMOERR_NOMEM;
                return (NULL);
        }
                
        return (raff);
}


