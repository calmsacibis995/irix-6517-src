/*
 * Copyright 1996-1997, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
#ident	"$Revision: 1.6 $"

#include <sys/types.h>
#include <ksys/as.h>
#include "as_private.h"
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/immu.h>
#include <sys/pfdat.h>
#include "pmap.h"
#include "region.h"
#include <sys/sysmacros.h>
#include "ksys/vpag.h"

/*
 * Subroutines to help with fault handling
 */
#ifdef CLNAA_DEBUG
#define Dcmn_err(x) 			cmn_err x
#else
#define Dcmn_err(x)
#endif

void
clnaasubr(pas_t *pas, preg_t *tprp, preg_t *first, caddr_t vaddr, pde_t *tpd)
{
	register preg_t 	*prp;
	register reg_t 		*rp = tprp->p_reg;
	uvaddr_t 	 	evaddr; /* equivalent vaddr in attached reg. */
	register pde_t 		*pd;
	pgno_t   		size, sz;
	int			nvalid;

	for (prp = first; prp; prp = PREG_NEXT(prp)) {
		if (prp == tprp) {
			Dcmn_err((CE_CONT, "clnAAptes:skip pas 0x%x pregion 0x%x\n",
					   pas, prp));
			continue;
		}
		if (prp->p_reg != rp)
			continue;
		Dcmn_err((CE_CONT, "clnAAptes:prp'=0x%x\n", prp));
		size = 1;
		evaddr = prp->p_regva + (vaddr - tprp->p_regva);
		pd = pmap_probe(prp->p_pmap, &evaddr, &size, &sz);

		if (pd == NULL) {
			/* not mapped: no work in this preg */
			Dcmn_err((CE_CONT, "clnAAptes:No ptes @ 0x%x\n",evaddr));
			continue;
		}

		if (pg_getpfn(pd) != pg_getpfn(tpd))  {
			/* mapped, but to a different page! weird */
			Dcmn_err((CE_CONT, "clnAAptes:pfn' (0x%x) no match\n",
				pg_getpfn(pd)));
			continue;
		}

		if (IS_LARGE_PAGE_PTE(pd))
			pmap_downgrade_lpage_pte(pas, evaddr, pd);

		Dcmn_err((CE_CONT, "clnAAptes: pas 0x%x preg 0x%x matches reg 0x%x"
		   "pfn (0x%x)\n", pas, prp, rp, pg_getpfn(pd)));

		nvalid = pmap_free(pas, prp->p_pmap, evaddr,1,PMAPFREE_FREE);
		prp->p_nvalid -= nvalid ;

		/*
		 * NOTE: Clean tlbs. if shaddr, _tlbunmap() takes care
		 * of dealing with all shared group processes ...
		 */
		tlbclean(pas, (__psunsigned_t)btoct(evaddr), pas->pas_isomask);
	}
}

/*
 * clnattachaddrptes - 
 *
 * vaddr - the address on which COW is being performed;
 * tpd - is the pde in pmap corresponding to vaddr.
 *
 * This function's main objective is to ensure that
 * there are no references to pfn(tpd) from the pmap of any thread
 * in the system that belongs to the passed in address space
 *
 * If sharing is done through PR_ATTACHADDR, then region structs
 * are the same, although pregions will be different.
 */
void
clnattachaddrptes(
	pas_t *pas,
	preg_t *tprp,
	uvaddr_t vaddr,
	pde_t *tpd)
{
	ppas_t *ppas;

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));

	ASSERT((tprp->p_regva <= vaddr)
		&& (vaddr < (tprp->p_regva + ctob(tprp->p_pglen))));

	Dcmn_err((CE_CONT, "clnAAptes: pas 0x%x prp=0x%x, rp=0x%x, vaddr=0x%x\n",
		pas, tprp, rp, vaddr));

	/* loop through all private pregion lists */
	mraccess(&pas->pas_plistlock);
	for (ppas = pas->pas_plist; ppas; ppas = ppas->ppas_next) {
		clnaasubr(pas, tprp, PREG_FIRST(ppas->ppas_pregions),
							vaddr, tpd);
	}
	mrunlock(&pas->pas_plistlock);

	/*
	 * Now clean any pointers from shared pregions
	 */
	clnaasubr(pas, tprp, PREG_FIRST(pas->pas_pregions), vaddr, tpd);
}
