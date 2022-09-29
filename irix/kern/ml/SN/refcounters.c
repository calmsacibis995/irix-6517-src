/*
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


#if SN0

/*
 * NUMA migration traffic control module 
 */

#include <sys/types.h>
#include <sys/cpumask.h>
#include <ksys/ddmap.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/SN/agent.h>
#include <sys/SN/router.h>
#include <sys/SN/hwcntrs.h>


/* 
 * Fault in pages for access to HUB HSPEC uncached space (directory space)
 * Called by: vfault()
 */

/* ARGSUSED */
int
refcounters_faulthandler(vhandl_t *vt, void *arg, uvaddr_t vaddr, int flags)
{
#if 0
	preg_t *pregion = vt->v_preg;
	reg_t	*rp = pregion->p_reg;
	pde_t *pde;
	int i;
	nasid_t nasid;
	__psunsigned_t pa;
	pgno_t	rpn, size, pfn;

	if (flags != W_READ)
		return (EACCES);
	/* Sync stuff */
	aspacelock(curprocp, MR_ACCESS);
	reglock(pregion->p_reg);

	pde = pmap_pte(pregion->p_pmap, vaddr, 0);
	ASSERT(pde);  /* Must get one */

#if 0
	nasid = COMPACT_TO_NASID_NODEID(master_node_get(dev_to_vhdl(hwgraph_vnode_to_dev(rp->r_vnode, curprocp->p_cred))));
#endif
	rpn = vtorpn(pregion, vaddr);
	pa = ctob(rpn) + rp->r_fileoff;
	if (pa >= rp->r_maxfsize) {
		regrele(pregion->p_reg);
		aspaceunlock(curprocp);
		return (EACCES);
	}
	size = btoc(min(rp->r_maxfsize - pa, NBPS - soff(pa))); 
	ASSERT(size > 0);
#if 0
	pa = NODE_BDDIR_BASE(nasid) | (pa & BDDIR_UPPER_MASK);
#endif
	pfn = btoc(pa);

	for(i=0; i < size; i++, pde++, pfn++) {
		/* 
	 	 * This page is always valid (PG_VR | PG_SV), 
		 * uncached (PG_TRUEUNCACHED), 
	 	 * belonging to HSPEC uncached space (PG_UC_HSPEC),
	 	 * non-replicable (special file),
	 	 * non-migratable (no reverse mapping),
	 	 * read-only page.
	 	 */
		pg_setpgi(pde, mkpde(PG_VR | PG_SV | PG_TRUEUNCACHED | PG_UC_HSPEC, pfn));

		pregion->p_nvalid++; /* It must be a valid page */
	}

	/* Sync stuff */
	regrele(pregion->p_reg);
	aspaceunlock(curprocp);

#endif
	return(0);
}
#endif /* SN0 */
