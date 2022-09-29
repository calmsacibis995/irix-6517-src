/*****************************************************************************
 * Copyright 1996, Silicon Graphics, Inc.
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
 ****************************************************************************/

#include <sys/types.h>
#include <sys/nodepda.h>
#include <sys/debug.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/hwgraph.h>
#include "pfms.h"
#include "migr_control.h"
#include  "numa_stats.h"


void
numa_stats_init(cnodeid_t node, caddr_t* nextbyte)
{
        NODEPDA(node)->numa_stats = (numa_stats_t*)
		low_mem_alloc(sizeof(numa_stats_t), nextbyte, "numa_stats");
        ASSERT(NODEPDA(node)->numa_stats);
}

int
numa_stats_get(char* nodepath, void* arg)
{
        vnode_t* vp;
        cnodeid_t apnode;
        
        if (nodepath == NULL) {
                apnode = CNODEID_NONE;
        } else {
                if (lookupname(nodepath,
                               UIO_USERSPACE,
                               FOLLOW,
                               NULLVPP,
                               &vp,
			       NULL)) {
                        return (ENOENT);
                }

                apnode = master_node_get(vp->v_rdev);
                VN_RELE(vp);
        }
        
        ASSERT((apnode == CNODEID_NONE) || (apnode >= 0 && apnode < numnodes));

         
        if (apnode == CNODEID_NONE) {
                return (EINVAL);
        }

        if (arg == NULL) {
                return (EFAULT);
        }

        if (copyout((void*)((NODEPDA((apnode)))->numa_stats), arg, sizeof(numa_stats_t))) {
                return (EFAULT);
        }

        return (0);
}

