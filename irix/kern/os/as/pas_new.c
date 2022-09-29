/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.19 $"
#include "sys/types.h"
#include "sys/anon.h"
#include "ksys/as.h"
#include "as_private.h"
#include "sys/atomic_ops.h"
#include "sys/cachectl.h"
#include "sys/cmn_err.h"
#include "sys/cred.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/kmem.h"
#include "limits.h"
#include "sys/lock.h"
#include <sys/numa.h>
#include "sys/proc.h"
#include "pmap.h"
#include "os/numa/pm.h"
#include "region.h"
#include "ksys/rmap.h"
#include "sys/vnode.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "ksys/vpag.h"

/*
 * create new AS's or attach new procs/sprocs/uthreads to an existing AS
 */
/*
 * getnewstk - get a new stack region for a sproc'ed process
 */
static uvaddr_t
getnewstk(pas_t *pas, ppas_t *ppas, size_t stksize)
{
	preg_t *prp;
	uvaddr_t start, end;

	/* find place to put stack
	 *
	 * We look for the highest address that process'es stack size will
	 * fit in. allocaddr() starts low, so this will give the least
	 * possible overlap.
	 * NOTE however, that this doesn't prevent someone from attaching
	 * between stacks and thereby preventing it from growing
	 */
	/*
	 * For speed, we maintain the lowest address that has
	 * been attached as a stack for this process. This
	 * is maintained here and in detachshaddr so that we reuse
	 * stacks.
	 */
	ASSERT(poff(stksize) == 0);
	end = pas->pas_lastspbase;
	ASSERT(poff(end) == 0);
	if ((size_t)end < stksize) {
		/* potentially out of room - reset everything */
		end = pas->pas_lastspbase = pas->pas_hiusrattach;
	}
	while ((size_t)end >= stksize) {
		start = end - stksize;
		if ((prp = findfpreg(pas, ppas, start, end - 1)) == NULL) {
			/* piece O' pie */
			pas->pas_lastspbase = start;
			/*
			 * We return end - we want to grow down and end
			 * is first unusable address
			 */
			ASSERT(poff(end) == 0);
			return end;
		}
		if (prp->p_type == PT_STACK) {
			end = prp->p_regva + ctob(prp->p_pglen) - prp->p_reg->r_maxfsize;
			if (end > prp->p_regva)
				end = prp->p_regva;
		} else
			end = prp->p_regva;
	}
	/*
	 * didn't find anything - blow off the hint and
	 * just try to find any address space large enough
	 */
	pas->pas_lastspbase = pas->pas_hiusrattach;
	if ((start = pas_allocaddr(pas, ppas, btoc(stksize), 0)) == NULL)
		return NULL;
	return (start + stksize);
}

static int
pas_newstack(pas_t *pas, ppas_t *ppas, size_t stklen, uvaddr_t *stkaddr)
{
	reg_t *rp;
	auto preg_t *prp;
	int error;
	uvaddr_t newstkaddr;
	struct pm *pm;

	ASSERT(mrislocked_update(&pas->pas_aspacelock));
	if ((newstkaddr = getnewstk(pas, ppas, stklen)) == NULL) {
		/* failed */
		return ENOMEM;
	}
	/* Allocate the region */
	rp = allocreg(NULL, RT_MEM, RG_ANON);

	/*
	 * Attach the region to this process.
	 * Must set PF_DUP flag in case of future fork.
	 * Permit mprotect to set executable
	 */
	ASSERT(poff(newstkaddr) == 0);
	pm = aspm_getdefaultpm(pas->pas_aspm, MEM_STACK);
	error = vattachreg(rp, pas, ppas, newstkaddr,
			(pgno_t) 0, rp->r_pgsz,
			PT_STACK, PROT_RW, PROT_RWX, PF_DUP, pm, &prp);
	aspm_releasepm(pm);
	if (error) {
		freereg(pas, rp);
		return error;
	}
	/* use r_maxfsize to store desired expansion room */
	rp->r_maxfsize = stklen;

	if (ppas->ppas_flags & PPAS_STACK)
		ppas->ppas_stksize = ctob(SINCR);
	else
		pas->pas_stksize = ctob(SINCR);

	if ((error = vgrowreg(prp, pas, ppas, PROT_RW, SINCR)) != 0) {
		if (error == EAGAIN)
			nomemmsg("sproc while initializing child stack");
		/* should be safe to detach - we're not a
		 * shared process yet
		 */
		(void) vdetachreg(prp, pas, ppas, prp->p_regva,
					prp->p_pglen, RF_NOFLUSH);
		return error;
	}
	if (ppas->ppas_flags & PPAS_STACK)
		ppas->ppas_stkbase = prp->p_regva;
	else
		pas->pas_stkbase = prp->p_regva;
	regrele(rp);
	*stkaddr = newstkaddr;

	return 0;
}

/*
 * prepare_share - the first time an AS can have multiple threads, we
 * need to do a bit of work
 */
static int
prepare_share(pas_t *pas, ppas_t *ppas)
{
	ASSERT(!(pas->pas_flags & PAS_SHARED));

	/*
	 * mark AS as being (potentially) shared.
	 * This means that private pregions will now
	 * use the ppas_pmap rather than
	 * the pas_pmap, and certain operations will be
	 * more expensive due to having multiple pmaps.
	 * pmap_split does all the pmap magic
	 */
	if (pmap_split(pas, ppas))
		return EAGAIN;

	pas_flagset(pas, PAS_SHARED);

	/*
	 * If we have private mappings, then there might
	 * be some unused segments in the shared pmap.
	 * Clean them up.  Clean out the wired tlb entries
	 * too since they could point to the pages
	 * pmap_trim frees.
	 * Any random entries are cleared when new_tlbpid
	 * is called later on.
	 */
	if (PREG_FIRST(ppas->ppas_pregions)) {
		if (pmap_trim(pas->pas_pmap)) {
			pas->pas_pmap->pmap_trimtime = 0;
			setup_wired_tlb(1);
		}
	}
	return 0;
}

/*
 * handle fork and sproc !(PR_SADDR) creations
 */
int
pas_fork(
	pas_t *ppas,			/* parent pas */
	ppas_t *pppas,			/* parent ppas */
	as_newres_t *res,		/* return info */
	uvaddr_t stkptr,		/* new stack pointer */
	size_t stklen,			/* sproc stack length */
	utas_t *cutas,			/* child's utas */
	as_newop_t what)
{
	preg_t		*p_prp;
	auto preg_t	*c_prp;
	pm_t		*pm;
	pas_t		*cpas;
	ppas_t		*cppas;
	int		doingshd;
	vasid_t		cvasid;
	rlim_t		stkmax;
	int		s, error;

	mrupdate(&ppas->pas_aspacelock);

	res->as_casid = as_create(cutas, &cvasid, ppas->pas_flags & PAS_64);
	res->as_cvasid = cvasid;
	cpas = VASID_TO_PAS(cvasid);
	cppas = (ppas_t *)cvasid.vas_pasid;

	cpas->pas_brkbase = ppas->pas_brkbase;
	cpas->pas_brksize = ppas->pas_brksize;
	cpas->pas_nextaalloc = ppas->pas_nextaalloc;
	cpas->pas_vmemmax = ppas->pas_vmemmax;
	cpas->pas_datamax = ppas->pas_datamax;
	cpas->pas_rssmax = ppas->pas_rssmax;
	cpas->pas_maxrss = ppas->pas_maxrss;
	cpas->pas_rss = ppas->pas_rss;

	/*
	 * Inherit the parent's process aggregate and start doing VM
	 * resource accounting. 
	 */
	if (ppas->pas_vpagg) {
		VPAG_HOLD(ppas->pas_vpagg);
		cpas->pas_vpagg = ppas->pas_vpagg;
	}

	cppas->ppas_rss = pppas->ppas_rss;
	cppas->ppas_flags |= (pppas->ppas_flags & (PPAS_ISO));
	if (pppas->ppas_flags & PPAS_STACK)
		stkmax = cpas->pas_stkmax = pppas->ppas_stkmax;
	else
		stkmax = cpas->pas_stkmax = ppas->pas_stkmax;
	/* initialize pmap for shared pregions */
	cpas->pas_pmap = pmap_create(ppas->pas_flags & PAS_64, PMAP_SEGMENT);

	ASSERT(ppas->pas_rssmax);

	/* init Policy Module - by default we inherit our parents */
	cpas->pas_aspm = aspm_dup(ppas->pas_aspm);
	mrupdate(&cpas->pas_aspacelock);

	if (stklen == AS_USE_RLIMIT) {
		/*
		 * if limit is 'infinite' let's use a reasonable
		 * value - that means they want no limit - not
		 * that they necessarily want a stack that big.
		 * Here, stklen is used to find a space stklen
		 * bytes long - clearly we'll never find
		 * an 'infinite' size.
		 */
		if (stkmax == RLIM_INFINITY)
			stklen = 64*1024*1024;
		else
			stklen = stkmax;
	}
	stklen = ctob(btoc(stklen));

	/*
	 * Duplicate all the regions of the process.
	 * Whether we actually make them copy-on-write depends on both
	 * their type and whether we're doing a fork or sproc.
	 * For sproc(), only shared pregions are checked
	 * For fork(), all pregions marked PF_DUP are made copy-on-write
	 * We single thread all forks/sprocs for a share group here
	 */

	/*
	 * If process has any saved regions (from exec) get rid of them now
	 * The main goal of ghost regions is to speed exec'ing - they
	 * potentially take up space/pages so holding on to them a long
	 * time seems counter-productive.
	 */
	if (ppas->pas_tsave)
		remove_tsave(ppas, pppas, RF_NOFLUSH);

	/*
	 * if we're a shared process now doing a fork - we must first
	 * effectively flush all shared processes' tlb entries - otherwise
	 * as we make things copy-on-write, other shared processes may
	 * still write them (it allows hardware tlb and software to be
	 * matched)
	 * They will all stop waiting for the shared pregion lock which we
	 * hold for update.
	 * Two consequences of calling newptes here:
	 *	1) the new_tlbpid below isn't necessary
	 *	2) all shared proceses's wired entries will be flushed
	 *	   even though they don't need to be
	 */
	if ((ppas->pas_flags & PAS_SHARED) && ((what == AS_NOSHARE_SPROC) == 0))
		newptes(ppas, pppas, 1);

	/*
	 * In this case, be sure and attach the shared regions first.
	 * A shared  address process may have two text regions --
	 * its original, and a RT_MEM one due to an getprivatespace().
	 * We need to attach the xdup'd region last, since
	 * otherwise chkgrowth() in attachreg() will report an error.
	 */
	p_prp = PREG_FIRST(ppas->pas_pregions);
	doingshd = 1;

doshd:
	for (; p_prp; p_prp = PREG_NEXT(p_prp)) {
		ASSERT(doingshd || (p_prp->p_flags & PF_PRIVATE));
		ASSERT(!doingshd || !(p_prp->p_flags & PF_PRIVATE));
		/* isolated regions are marked RG_ISOLATE */
		ASSERT(p_prp->p_reg->r_flags & RG_ISOLATE || \
		 	((pppas->ppas_flags & PPAS_ISO) == 0));
		if (doingshd) {
			/* look to see if pregion matches one on private list */
			preg_t *tprp;
			for (tprp = PREG_FIRST(pppas->ppas_pregions); tprp; tprp = PREG_NEXT(tprp)){
				if (p_prp->p_regva == tprp->p_regva) {
					ASSERT((tprp->p_flags & (PF_DUP|PF_NOSHARE)) 
						== (PF_DUP|PF_NOSHARE));
					goto skippreg;
				}
			}
		}

		/*
		 * If we're doing an sproc (and sharing address space) then
		 * all regions getting here
		 * will be placed on child's private pregion list
		 * (cppas_pregions) since they are marked PF_PRIVATE
		 * We always turn off 'PF_SHARED' since either its going
		 * on the private list or in a new -non-shared AS.
		 */
		if (p_prp->p_type == PT_GR)
			continue;

		reglock(p_prp->p_reg);
#ifdef R10000_SPECULATION_WAR
		if (p_prp->p_reg->r_flags & RG_LOCKED) {
			fixlockedpreg(ppas, pppas, p_prp, 1);
		}
#endif
                pm = aspm_getdefaultpm(cpas->pas_aspm, MEM_DATA);
		error = dupreg(ppas, p_prp, cpas, cppas, p_prp->p_regva, 
				p_prp->p_type, p_prp->p_flags & ~PF_SHARED, 
				pm, 0, &c_prp);
                aspm_releasepm(pm);
#ifdef R10000_SPECULATION_WAR
		if (p_prp->p_reg->r_flags & RG_LOCKED) {
			if (c_prp)
				duplockedpreg(cpas, cppas, c_prp, p_prp);
			fixlockedpreg(ppas, pppas, p_prp, 0);
		}
#endif
		if (error) {
			regrele(p_prp->p_reg);
			goto bad;
		}

		if (c_prp->p_reg != p_prp->p_reg)
			regrele(c_prp->p_reg);
		regrele(p_prp->p_reg);

skippreg:;
	}

	/*
	 * After attaching the shared regions, now attach the private ones.
	 */
	if (doingshd) {
		p_prp = PREG_FIRST(pppas->ppas_pregions);
		doingshd = 0;
		goto doshd;
	}

 	if ((what == AS_NOSHARE_SPROC) && stkptr == NULL) {
		error = pas_newstack(cpas, cppas, stklen, &res->as_stkptr);
		if (error == EAGAIN) {
			nomemmsg("sproc while initializing child stack");
			goto bad;
		}
	} else if (what == AS_NOSHARE_SPROC) {
		/* user provided stack */
		res->as_stkptr = (uvaddr_t)((__psunsigned_t)stkptr & ~0xF);
	} else {
		/* fork */
		if (pppas->ppas_flags & PPAS_STACK) {
			cpas->pas_stkbase = pppas->ppas_stkbase;
			cpas->pas_stksize = pppas->ppas_stksize;
		} else {
			cpas->pas_stkbase = ppas->pas_stkbase;
			cpas->pas_stksize = ppas->pas_stksize;
		}
	}
	res->as_pm = aspm_getdefaultpm(cpas->pas_aspm, MEM_DATA);
	mrunlock(&cpas->pas_aspacelock);
	mrunlock(&ppas->pas_aspacelock);

	return 0;
bad:
	/*
	 * detach all so-far attached regions
	 */
	while (c_prp = PREG_FIRST(cpas->pas_pregions)) {
		reglock(c_prp->p_reg);
		(void) vdetachreg(c_prp, cpas, cppas,
			c_prp->p_regva, c_prp->p_pglen, RF_NOFLUSH);
	}
	while (c_prp = PREG_FIRST(cppas->ppas_pregions)) {
		reglock(c_prp->p_reg);
		(void) vdetachreg(c_prp, cpas, cppas, c_prp->p_regva,
					c_prp->p_pglen, RF_NOFLUSH);
	}
	ASSERT(cpas->pas_refcnt >= 1);
	atomicAddInt(&cpas->pas_refcnt, -1);
	/* no longer permitted to reference uthread */
	s = mutex_spinlock(&cppas->ppas_utaslock);
	cppas->ppas_utas = NULL;
	mutex_spinunlock(&cppas->ppas_utaslock, s);

	mrunlock(&ppas->pas_aspacelock);
	mrunlock(&cpas->pas_aspacelock);
	as_rele(cvasid); /* may be last ref */
	return error;
}

int
pas_sproc(
	pas_t *ppas,			/* parent pas */
	ppas_t *pppas,			/* parent ppas */
	as_newres_t *res,		/* return info */
	uvaddr_t stkptr,		/* new stack pointer */
	size_t stklen,			/* sproc stack length */
	utas_t *cutas)			/* new thread's (child) utas */
{
	preg_t		*p_prp;
	auto preg_t	*c_prp;
	pm_t		*pm;
	pas_t		*cpas;
	ppas_t		*cppas;
	vasid_t		cvasid;
	rlim_t		stkmax;
	int		s, error;

	mrupdate(&ppas->pas_aspacelock);

	if (private.p_flags & PDAF_ISOLATED) {
		/*
		 * XXX scheduler seems to sometimes run
		 * non-isolated processes on isolated processors...
		 */
		ASSERT(CPUMASK_IS_ZERO(ppas->pas_isomask) || \
		       CPUMASK_TSTB(ppas->pas_isomask, private.p_cpuid));

	}

	/* if first time sharing do some special work */
	if (!(ppas->pas_flags & PAS_SHARED)) {
		if (error = prepare_share(ppas, pppas)) {
			mrunlock(&ppas->pas_aspacelock);
			return error;
		}

		ppas_flagset(pppas, PPAS_SADDR);

		/*
		 * It's ok to leave originating sproc !PPAS_STACK --
		 * it will be the only one using the shared pas
		 * stack info.
		 */
#ifdef not_needed
		pppas->ppas_stksize = ppas->pas_stksize;
		pppas->ppas_stkbase = ppas->pas_stkbase;
#endif
	}

	/* allocate && attach new private portion of address space */
	ppas_alloc(ppas, pppas, &cvasid, &res->as_casid, cutas);

	/*
	 * at this point cvasid and pvasid point to the same
	 * vas which has a reference for both of them
	 */
	cpas = VASID_TO_PAS(cvasid);
	ASSERT(cpas == ppas);
	cppas = (ppas_t *)cvasid.vas_pasid;

	/* inherit per-sproc stkmax */
	cppas->ppas_flags |= PPAS_SADDR|PPAS_STACK;
	if (pppas->ppas_flags & PPAS_STACK)
		stkmax = cppas->ppas_stkmax = pppas->ppas_stkmax;
	else
		stkmax = cppas->ppas_stkmax = ppas->pas_stkmax;

	if (stklen == AS_USE_RLIMIT) {
		/*
		 * if limit is 'infinite' let's use a reasonable
		 * value - that means they want no limit - not
		 * that they necessarily want a stack that big.
		 * Here, stklen is used to find a space stklen
		 * bytes long - clearly we'll never find
		 * an 'infinite' size.
		 */
		if (stkmax == RLIM_INFINITY)
			stklen = 64*1024*1024;
		else
			stklen = stkmax;
	}
	stklen = ctob(btoc(stklen));

	/*
	 * Duplicate all the regions of the process.
	 * Whether we actually make them copy-on-write depends on both
	 * their type and whether we're doing a fork or sproc.
	 * For sproc(), only shared pregions are checked
	 * For fork(), all pregions marked PF_DUP are made copy-on-write
	 * We single thread all forks/sprocs for a share group here
	 */

	/*
	 * If process has any saved regions (from exec) get rid of them now
	 * The main goal of ghost regions is to speed exec'ing - they
	 * potentially take up space/pages so holding on to them a long
	 * time seems counter-productive.
	 */
	if (ppas->pas_tsave)
		remove_tsave(ppas, pppas, RF_NOFLUSH);

#if DEBUG
	p_prp = PREG_FIRST(ppas->pas_pregions);
	for (; p_prp; p_prp = PREG_NEXT(p_prp)) {
		ASSERT(!(p_prp->p_flags & PF_PRIVATE));
		/* isolated regions are marked RG_ISOLATE */
		ASSERT(p_prp->p_reg->r_flags & RG_ISOLATE || \
		 	((pppas->ppas_flags & PPAS_ISO) == 0));
		ASSERT(((p_prp->p_flags & PF_NOSHARE) == 0) &&
					(REG_REPLICABLE(p_prp->p_reg) == 0));
	}
#endif
	for (p_prp = PREG_FIRST(pppas->ppas_pregions);
	     p_prp;
	     p_prp = PREG_NEXT(p_prp)) {

		ASSERT(p_prp->p_flags & PF_PRIVATE);
		/* isolated regions are marked RG_ISOLATE */
		ASSERT(p_prp->p_reg->r_flags & RG_ISOLATE || \
		 	((pppas->ppas_flags & PPAS_ISO) == 0));

		/*
		 * If we're doing an sproc (and sharing address space) then
		 * all regions getting here
		 * will be placed on child's private pregion list
		 * (cppas_pregions) since they are marked PF_PRIVATE
		 * We always turn off 'PF_SHARED' since either its going
		 * on the private list or in a new -non-shared AS.
		 */
		if (p_prp->p_type == PT_GR && (p_prp->p_flags & PF_NOSHARE))
			continue;

		reglock(p_prp->p_reg);
#ifdef R10000_SPECULATION_WAR
		if (p_prp->p_reg->r_flags & RG_LOCKED) {
			fixlockedpreg(ppas, pppas, p_prp, 1);
		}
#endif
                pm = aspm_getdefaultpm(cpas->pas_aspm, MEM_DATA);
		error = dupreg(ppas, p_prp, cpas, cppas, p_prp->p_regva,
				p_prp->p_type, p_prp->p_flags & ~PF_SHARED,
				pm, 0, &c_prp);
                aspm_releasepm(pm);

#ifdef R10000_SPECULATION_WAR
		if (p_prp->p_reg->r_flags & RG_LOCKED) {
			if (c_prp)
				duplockedpreg(cpas, cppas, c_prp, p_prp);
			fixlockedpreg(ppas, pppas, p_prp, 0);
		}
#endif
		if (error) {
			regrele(p_prp->p_reg);
			goto bad;
		}

		if (c_prp->p_reg != p_prp->p_reg)
			regrele(c_prp->p_reg);
		regrele(p_prp->p_reg);

	}

 	if (stkptr == NULL) {
		if (error = pas_newstack(cpas, cppas, stklen, &res->as_stkptr)) {
			if (error == EAGAIN)
				nomemmsg("sproc while initializing child stack");
			goto bad;
		}
	} else {
		/* user provided stack */
		res->as_stkptr = (uvaddr_t)((__psunsigned_t)stkptr & ~0xF);
	}
	res->as_pm = aspm_getdefaultpm(cpas->pas_aspm, MEM_DATA);
	mrunlock(&ppas->pas_aspacelock);

	return 0;
bad:
	/*
	 * detach all so-far attached regions
	 * just detach private regions
	 */
	while (c_prp = PREG_FIRST(cppas->ppas_pregions)) {
		reglock(c_prp->p_reg);
		(void) vdetachreg(c_prp, cpas, cppas, c_prp->p_regva,
					c_prp->p_pglen, RF_NOFLUSH);
	}
	ASSERT(cpas->pas_refcnt >= 1);
	atomicAddInt(&cpas->pas_refcnt, -1);
	/* no longer permitted to reference uthread */
	s = mutex_spinlock(&cppas->ppas_utaslock);
	cppas->ppas_utas = NULL;
	mutex_spinunlock(&cppas->ppas_utaslock, s);

	mrunlock(&ppas->pas_aspacelock);
	as_rele(cvasid);
	return error;
}

int
pas_uthread(
	pas_t *ppas,			/* parent pas */
	ppas_t *pppas,			/* parent ppas */
	as_newres_t *res,		/* return info */
	utas_t *cutas)			/* new thread's (child) utas */
{
	preg_t		*p_prp;
	auto preg_t	*c_prp;
	pm_t		*pm;
	pas_t		*cpas;
	ppas_t		*cppas;
	vasid_t		cvasid;
	int		s, error;

	ASSERT(!(pppas->ppas_flags & (PPAS_SADDR|PPAS_STACK)));
	mrupdate(&ppas->pas_aspacelock);

#ifdef DEBUG
	if (private.p_flags & PDAF_ISOLATED) {
		/*
		 * XXX scheduler seems to sometimes run
		 * non-isolated processes on isolated processors...
		 */
		ASSERT(CPUMASK_IS_ZERO(ppas->pas_isomask) || \
		       CPUMASK_TSTB(ppas->pas_isomask, private.p_cpuid));

	}
#endif

	/* if first time sharing do some special work */
	if (!(ppas->pas_flags & PAS_SHARED)) {
		if (error = prepare_share(ppas, pppas)) {
			mrunlock(&ppas->pas_aspacelock);
			return error;
		}
	}

	/* allocate && attach new private portion of address space */
	ppas_alloc(ppas, pppas, &cvasid, &res->as_casid, cutas);

	/*
	 * at this point cvasid and pvasid point to the same
	 * vas which has a reference for both of them
	 */
	cpas = VASID_TO_PAS(cvasid);
	ASSERT(cpas == ppas);
	cppas = (ppas_t *)cvasid.vas_pasid;

	/*
	 * If process has any saved regions (from exec) get rid of them now
	 * The main goal of ghost regions is to speed exec'ing - they
	 * potentially take up space/pages so holding on to them a long
	 * time seems counter-productive.
	 */
	if (ppas->pas_tsave)
		remove_tsave(ppas, pppas, RF_NOFLUSH);

#if DEBUG
	for (p_prp = PREG_FIRST(ppas->pas_pregions);
	     p_prp;
	     p_prp = PREG_NEXT(p_prp)) {
		ASSERT(!(p_prp->p_flags & PF_PRIVATE));
		/* isolated regions are marked RG_ISOLATE */
		ASSERT(p_prp->p_reg->r_flags & RG_ISOLATE || \
		 	((pppas->ppas_flags & PPAS_ISO) == 0));
		ASSERT(((p_prp->p_flags & PF_NOSHARE) == 0) &&
					(REG_REPLICABLE(p_prp->p_reg) == 0));
	}
#endif

	/*
	 * loop through all private regions to see if any should be
	 * dupped
	 */
	for (p_prp = PREG_FIRST(pppas->ppas_pregions);
	     p_prp;
	     p_prp = PREG_NEXT(p_prp)) {

		ASSERT(p_prp->p_flags & PF_PRIVATE);
		/* isolated regions are marked RG_ISOLATE */
		ASSERT(p_prp->p_reg->r_flags & RG_ISOLATE || \
		 	((pppas->ppas_flags & PPAS_ISO) == 0));

		/*
		 * If we're doing an sproc (and sharing address space) then
		 * all regions getting here
		 * will be placed on child's private pregion list
		 * (cppas_pregions) since they are marked PF_PRIVATE
		 * We always turn off 'PF_SHARED' since either its going
		 * on the private list or in a new -non-shared AS.
		 */
		if (p_prp->p_type == PT_GR && (p_prp->p_flags & PF_NOSHARE))
			continue;

		reglock(p_prp->p_reg);
#ifdef R10000_SPECULATION_WAR
		if (p_prp->p_reg->r_flags & RG_LOCKED) {
			fixlockedpreg(ppas, pppas, p_prp, 1);
		}
#endif
                pm = aspm_getdefaultpm(cpas->pas_aspm, MEM_DATA);
		error = dupreg(ppas, p_prp, cpas, cppas, p_prp->p_regva,
				p_prp->p_type, p_prp->p_flags & ~PF_SHARED,
				pm, 0, &c_prp);
                aspm_releasepm(pm);

#ifdef R10000_SPECULATION_WAR
		if (p_prp->p_reg->r_flags & RG_LOCKED) {
			if (c_prp)
				duplockedpreg(cpas, cppas, c_prp, p_prp);
			fixlockedpreg(ppas, pppas, p_prp, 0);
		}
#endif
		if (error) {
			regrele(p_prp->p_reg);
			goto bad;
		}

		if (c_prp->p_reg != p_prp->p_reg)
			regrele(c_prp->p_reg);
		regrele(p_prp->p_reg);

	}

	/*
	 * let caller mess with stack pointer....
	 */
	res->as_pm = aspm_getdefaultpm(cpas->pas_aspm, MEM_DATA);
	mrunlock(&ppas->pas_aspacelock);

	return 0;

bad:
	/*
	 * detach all so-far attached regions
	 * just detach private regions
	 */
	while (c_prp = PREG_FIRST(cppas->ppas_pregions)) {
		reglock(c_prp->p_reg);
		(void) vdetachreg(c_prp, cpas, cppas, c_prp->p_regva,
					c_prp->p_pglen, RF_NOFLUSH);
	}
	ASSERT(cpas->pas_refcnt >= 1);
	atomicAddInt(&cpas->pas_refcnt, -1);

	/* no longer permitted to reference uthread */
	s = mutex_spinlock(&cppas->ppas_utaslock);
	cppas->ppas_utas = NULL;
	mutex_spinunlock(&cppas->ppas_utaslock, s);

	mrunlock(&ppas->pas_aspacelock);
	as_rele(cvasid);
	return error;
}
