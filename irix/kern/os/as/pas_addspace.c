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

#ident	"$Revision: 3.214 $"
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
#include "region.h"
#include "sys/vnode.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/buf.h"
#include "ksys/vpag.h"
#include "ksys/vmmacros.h"
#include "sys/miser_public.h"
#include "os/proc/pproc_private.h"
#include "sys/prctl.h"


extern dev_t zeroesdev;
#ifdef CKPT
extern int ckpt_enabled;
#endif

static int isshdmember(asid_t);

/*
 * check validity of args for FIXED mapping,
 * and do a 'stupid' unmapping of anything that's there
 */
/* ARGSUSED */
static int
chkfixed(pas_t *pas, ppas_t *ppas, uvaddr_t vaddr, size_t len, int flags, 
	vnode_t *vp, off_t off, mprot_t prot)
{
	if ((__psint_t)vaddr & POFFMASK)
		return EINVAL;
	
#ifdef _VCE_AVOIDANCE
	if (vce_avoidance) {
		/*
		 * Require that read-write mappings of regular and
		 * block device files be color-congruent to the
		 * file, so avoid color conflicts with the buffer
		 * cache.
		 */
		if (vp->v_type != VCHR &&
		    (prot & PROT_WRITE) &&
		    ((flags & MAP_TYPE) == MAP_SHARED) &&
		    (colorof(off) != colorof(vaddr))) {
			return EINVAL;
		}
	}
#endif /* _VCE_AVOIDANCE */
	if (vaddr < (uvaddr_t)LOWUSRATTACH ||
	    (vaddr + len) > pas->pas_hiusrattach)
		return ENOMEM;

	/*	Do a stupid unmapping here.  If it fails
	 *	because the address space isn't currently
	 *	in use, ok.  If it fails because of address
	 *	space conflict, the subsequent growreg will fail.
	 */
	(void)pas_unmap(pas, ppas, vaddr, len,
			flags & MAP_LOCAL ? DEL_MUNMAP_LOCAL : 0);

	/*
	 * If this is a MAP_LOCAL mapping and we're sharing our address
	 * space, it could be overlapping an existing shared region.
	 * In this case, the munmap above would not have detached
	 * anything which means that we could still have tlb entries
	 * pointing to the shared pages.  Nuke them from the tlb here.
	 * We could actually check to see if there is such an overlap,
	 * but MAP_LOCAL and MAP_FIXED seems like a rare enough case
	 * that it's not worth the effort.
	 */
	if ((flags & MAP_LOCAL) && (pas->pas_flags & PAS_SHARED))
		flushtlb_current();
	return 0;
}

/*
 * Add an mmap
 */
int
pas_addmmap(pas_t *pas, ppas_t *ppas, as_addspace_t *arg, uvaddr_t *ataddr)
{
        struct pm *pm;
	int error = 0;
	mprot_t prot;
	struct region *rp;
	preg_t *prp;
	int pflags = 0, flags, type;
	ushort regflags = 0;
	uvaddr_t vaddr;
	vnode_t *vp;
	ssize_t len;
	off_t off;

	flags = arg->as_mmap_flags;
	type = flags & MAP_TYPE;
	prot = arg->as_prot;
	vaddr = arg->as_addr;
	vp = arg->as_mmap_vp;
	off = arg->as_mmap_off;
	len = arg->as_length;

	ASSERT(poff(off) == 0);
	if (flags & MAP_AUTOGROW)
		regflags |= RG_AUTOGROW;
	if (flags & MAP_AUTORESRV)
		regflags |= RG_AUTORESRV;
	if (flags & MAP_TEXT)
		regflags |= RG_TEXT;
	if (flags & MAP_LOCAL)
		pflags |= PF_NOSHARE;

	if (type == MAP_PRIVATE) {
		regflags |= RG_CW | RG_ANON;
		pflags |= PF_DUP;
	}
	mrupdate(&pas->pas_aspacelock);

	/* Would we exceed process memory limit? */
	if (chkpgrowth(pas, ppas, btoc(len)) < 0) {
		mrunlock(&pas->pas_aspacelock);
		return ENOMEM;
	}

	/* For MAP_FIXED, addr must be a multiple of page size */
	if (flags & MAP_FIXED) {
		if (error = chkfixed(pas, ppas, vaddr, len, flags, vp, off, prot)) {
			mrunlock(&pas->pas_aspacelock);
			return error;
		}
	}

	/*
	 * allocate addr if:
	 * 1) user didn't specify FIXED and didn't specify a hint
	 * 2) tried hint and failed
	 */
getaddr:
	if (vaddr == NULL && !(flags & MAP_FIXED)) {
		vaddr = pas_allocaddr(pas, ppas, btoc(len), colorof(off));

		if (vaddr == NULL) {
			mrunlock(&pas->pas_aspacelock);
			return ENOMEM;
		}

		flags |= MAP_FIXED;
	}
	ASSERT(vaddr || (flags & MAP_FIXED));

	/* for MMAP we don't bother with tsave regions */
	rp = allocreg(vp, flags & MAP_SHARED ? RT_MAPFILE : RT_MEM, regflags);

	pm = aspm_getdefaultpm(pas->pas_aspm, MEM_DATA); 
	if (error = vattachreg(rp, pas, ppas, vaddr, (pgno_t)0, 0, PT_MAPFILE, 
			      prot, arg->as_maxprot, pflags, pm, &prp)) {
		aspm_releasepm(pm);
		freereg(pas, rp);
		if (!(flags & MAP_FIXED)) {
			vaddr = NULL;
			goto getaddr;
		}
		mrunlock(&pas->pas_aspacelock);
		return error;
	}
	aspm_releasepm(pm);
	
	rp->r_maxfsize = off + len;
	rp->r_fileoff = off;

	if (error = vgrowreg(prp, pas, ppas, prot,
				btoc(len + (vaddr - prp->p_regva)))) {
		vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen, 
								  RF_NOFLUSH);
		if (error == ENOMEM && !(flags & MAP_FIXED)) {
			/*
			 * Must be first time around user tried to give a vaddr
			 * hint, which didn't work; forget it and startover.
			 */
			vaddr = NULL;
			goto getaddr;
		}
		mrunlock(&pas->pas_aspacelock);
		return error;
	}
#ifdef CKPT
	if (ckpt_enabled) {
		rp->r_ckptflags = CKPT_MMAP;
		if (type == MAP_PRIVATE)
			rp->r_ckptflags |= CKPT_PRIVATE;
		rp->r_ckptinfo = arg->as_mmap_ckpt;
	}
#endif
	regrele(rp);
	mrunlock(&pas->pas_aspacelock);

	/*
	 * Lock down the region pages if future page locking is enabled
	 * for the process. Note: autogrow region pages are locked down
	 * as they are faulted in, not here.
	 */
	if ((len > 0) && (pas->pas_lockdown & AS_FUTURELOCK)) {
		struct vattr va;
		size_t size;

		va.va_mask = AT_SIZE;
		VOP_GETATTR(vp, &va, 0, get_current_cred(), error);
		if (error)
			goto badlock;
		size = MIN(len, va.va_size);
		mraccess(&pas->pas_aspacelock);
		ASSERT(!(rp->r_flags & RG_PHYS));
		if (lockpages2(pas, ppas, vaddr, btoc(size), LPF_NONE)) {
			/*
			 * The locking down of region pages failed. 
			 * Deallocate the region.
			 */
			mrunlock(&pas->pas_aspacelock);
			error = ENOMEM;
badlock:
			mrupdate(&pas->pas_aspacelock);
			(void) pas_unmap(pas, ppas, vaddr, len,
				flags & MAP_LOCAL ? DEL_MUNMAP_LOCAL : 0);
			mrunlock(&pas->pas_aspacelock);
			return error;
		}
		mrunlock(&pas->pas_aspacelock);
	}
	*ataddr = vaddr;
	return 0;
}

#if MH_R10000_SPECULATION_WAR && DEBUG
#include <os/vm/anon_mgr.h>
#endif /* MH_R10000_SPECULATION_WAR */

int
pas_addmmapdevice(pas_t *pas, ppas_t *ppas, as_addspace_t *arg, uvaddr_t *ataddr)
{
	size_t len;
	int flags;
	int type;
	mprot_t prot;
	uvaddr_t vaddr;
	vnode_t *vp;
	off_t off;
        struct pm *pm;
	preg_t *prp;
	reg_t *rp;
	unchar reg_type = RT_MAPFILE;
	ushort reg_flags = RG_PHYS;
	unchar preg_flags = 0;
	int error, iszero;
	vhandl_t vt;
	extern int	enable_devzero_opt;

	flags = arg->as_mmap_flags;
	type = flags & MAP_TYPE;
	prot = arg->as_prot;
	vaddr = arg->as_addr;
	vp = arg->as_mmap_vp;
	off = arg->as_mmap_off;
	len = arg->as_length;

	if (type == MAP_PRIVATE) {
		reg_type = RT_MEM;
		preg_flags = PF_DUP;
	}
	if (flags & MAP_LOCAL)
		preg_flags |= PF_NOSHARE;
	if (flags & MAP_AUTOGROW)
		reg_flags |= RG_AUTOGROW;
	if (flags & MAP_AUTORESRV)
		reg_flags |= RG_AUTORESRV;

	ASSERT(vp->v_type == VCHR || vp->v_type == VBLK);

	/* flag if we are doing /dev/zero */
	iszero = vp->v_rdev == zeroesdev;
	if (iszero) {
		reg_type = RT_MEM;
		reg_flags &= ~RG_PHYS;
		reg_flags |= RG_ANON|RG_MAPZERO;
	}

	mrupdate(&pas->pas_aspacelock);

	/* Would we exceed process memory limit? */
	if (chkpgrowth(pas, ppas, btoc(len)) < 0) {
		mrunlock(&pas->pas_aspacelock);
		return ENOMEM;
	}

	/* For MAP_FIXED, addr must be a multiple of page size */
	if (flags & MAP_FIXED) {
		/*
		 * check for errors and do 'stupid' unmapping
		 */
		if (error = chkfixed(pas, ppas, vaddr, len, flags, vp, off, prot)) {
			mrunlock(&pas->pas_aspacelock);
			return error;
		}
	}

getaddr:
	if (vaddr == NULL && !(flags & MAP_FIXED)) {
		vaddr = pas_allocaddr(pas, ppas, btoc(len), colorof(off));

		if (vaddr == NULL) {
			mrunlock(&pas->pas_aspacelock);
			return ENOMEM;
		}

		flags |= MAP_FIXED;
	}

	/*
	 * Optimization for /dev/zero mappings. Try to grow the
	 * region instead of allocating a new one.
	 */
	if (iszero)  {
		prp = findfpreg_select(pas, ppas, (vaddr - NBPP), 
				(vaddr - 1), flags & MAP_LOCAL);
		if (prp && (prp->p_type == PT_MAPFILE) && enable_devzero_opt) {
			pgno_t rpn;
			int tmp_reg_flags = reg_flags & 
				(RG_AUTOGROW|RG_AUTORESRV);
			int tmp_preg_flags = preg_flags &  PF_DUP;

			rp = prp->p_reg;
			rpn = vtorpn(prp, vaddr);

		/*
		 * We can grow the region only if the following conditions
		 * are satisfied.
		 * 1) There is only one pregion using the region.
		 * 2) It has nodma going on.
		 * 3) It is a map zero region
		 * 4) the region is not bigger than the pregion using it 
		 */

			if ((rp->r_flags & RG_MAPZERO) && 
				(rp->r_refcnt == 1) && 
				(prp->p_noshrink == 0) && 
				(rpn == rp->r_pgsz) &&
				(tmp_reg_flags == (rp->r_flags & 
					(RG_AUTOGROW|RG_AUTORESRV))) &&
				(tmp_preg_flags == (prp->p_flags & PF_DUP))) {

				reglock(rp);

				/* just grow region */
				error = vgrowreg(prp, pas, ppas, prot, btoc(len));
				if (error == 0)
					rp->r_mappedsize += len;
				regrele(rp);
				if (error == ENOMEM && !(flags & MAP_FIXED)) {
					vaddr = NULL;
					goto getaddr;
				}
				mrunlock(&pas->pas_aspacelock);
				if (error)
					return error;
				*ataddr = vaddr;
				return 0;
			}
		}
		/*
		 * want a /dev/zero mapping, but didn't find one to grow
		 *
		 * Since we'll be dumping the vp, make sure RG_AUTOGROW
		 * isn't set.
		 */
		reg_flags &= ~RG_AUTOGROW;
	}

	/*
	 * Allocate the region structure;
	 * reg_type is either RT_MAPFILE or RT_MEM.
	 * Optimization for /dev/zero  - no need for vp - and who
	 * wants the long mreg list???
	 */
	rp = allocreg(iszero ? NULL : vp, reg_type, reg_flags);

	/*	Attach the region to pregion list.
	 *	preg_prots indicate protections.
	 *	preg_flags indicate whether region is to be dup'd on a fork.
	 */
        pm = aspm_getdefaultpm(pas->pas_aspm, MEM_DATA);
	error = vattachreg(rp, pas, ppas, vaddr, (pgno_t) 0, rp->r_pgsz,
			PT_MAPFILE, prot, arg->as_maxprot,
			preg_flags, pm, &prp);
        aspm_releasepm(pm);
	if (error) {
		freereg(pas, rp);
		if (!(flags & MAP_FIXED)) {
			/*
			 * Must be first time around user tried to give a vaddr
			 * hint, which didn't work; forget it and startover.
			 */
			vaddr = NULL;
			goto getaddr;
		}
		mrunlock(&pas->pas_aspacelock);
		return error;
	}

	/*
	 * go ahead and grow region to desired length
	 */
	if (error = vgrowreg(prp, pas, ppas, prot, btoc(len))) {
		(void) vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen,
				 RF_NOFLUSH);
		/* unlocks rp */

		/*
		 * if we couldn't do the grow because another
		 * region was in the way, then try a different address if 
		 * MAP_FIXED was not set (we do this at most once
		 * since we're holding aspacelock).
		 */
		if (error == ENOMEM && !(flags & MAP_FIXED)) {
			vaddr = NULL;
			goto getaddr;
		}
		mrunlock(&pas->pas_aspacelock);

		return error;
	}
	rp->r_maxfsize = len;
	ASSERT(vaddr == prp->p_regva);


	/*
	 * If we're mapping zero - we're all done
	 */
	if (iszero)
		goto done;

	/*
	 * Ok, now let the vnode map routine do its thing to set things up.
	 * We set mappedsize once we know that spec has set its
	 * mapcnt ....
	 */
	vt.v_preg = prp;
	vt.v_addr = vaddr;
	VOP_ADDMAP(vp, VNEWMAP, &vt, NULL, off, len, prot, get_current_cred(),
							error);
	if (error == EBADE) {
		/*
		 * sigh - its an SVR4-style mmap interface - time for
		 * lots of looping!
		 */
		pgcnt_t npages;
		auto pgno_t pgno;
		pgi_t bits;
		pde_t *pte;
		uvaddr_t tvaddr = vaddr;
#if _NO_UNCACHED_MEM_WAR	/* IP26/IP28 */
		extern int ip26_allow_ucmem;
		int is_in_pfdat(pgno_t);
#endif

		rp->r_mappedsize = len;
		npages = btoc(len);
		error = 0;
		if (prot) {
			if (prot & PROT_WRITE)
				bits = PG_VR | PG_SV | PG_M;
			else
				bits = PG_VR | PG_SV;
		} else
			bits = 0;
		bits = pte_noncached(bits);

		while (--npages >= 0) {
			VOP_ADDMAP(vp, VITERMAP, NULL, &pgno,
					off, NBPP, prot, get_current_cred(),
					error);
			if (error)
				break;
			pte = pmap_pte(pas, prp->p_pmap, tvaddr, 0);
#if _NO_UNCACHED_MEM_WAR	/* IP26/IP28 */
#if defined(IP26) && (_PAGESZ != 16384)
#error IP26 mmap needs to know cache color.
#endif
			/*  The Power Indigo2 cannot write to memory
			 * uncached.  Since the pagesize == the cache
			 * size we do not have to worry about mapping
			 * color.  If stuned to allow uncached memory
			 * deal page uncached like other systems.
			 *
			 *  IP28 is the same way, but T5 handles
			 * virtual coherency problems in hardware.
			 */
			if (is_in_pfdat(pgno) && !ip26_allow_ucmem)
				pg_setpgi(pte, mkpde(pte_cached(bits),pgno));
			else
#endif
			pg_setpgi(pte, mkpde(bits, pgno));

			tvaddr += NBPP;
			off += NBPP;
			prp->p_nvalid++;
		}
	} else if (error == 0)
		rp->r_mappedsize = len;
	if (error) {
		(void) vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen,
				 RF_NOFLUSH);
		/* unlocks rp */
		mrunlock(&pas->pas_aspacelock);
		return error;
	}
	/*
	 * Some insidious drivers reset rp,
	 * others may change r_type or set RG_PHYS.
	 */
	rp = prp->p_reg;

	/* Device drivers could either map device physical
	 * space or system memory into user space. 
	 * RG_PHYS will be used to represent either of these
	 * cases to retain its current meaning.
	 * When drivers map physical device space, we will use
	 * an additional flag RG_PHYSIO to represent the 
	 * region as an IO region. 
	 */
	{
	pde_t	*pdep;
	pgno_t	size = prp->p_pglen, pglen;
	int 	phys_reg = 0;
	caddr_t	vaddr;

	vaddr = prp->p_regva;
	do {
		pglen = size;
		pdep  = pmap_ptes(pas, prp->p_pmap, vaddr, &pglen, 0);
		size -= pglen;
		for (; pglen > 0; pglen--, pdep++) {
			if (pg_isnoncache(pdep)){
				phys_reg = 1;
				break;
			}
		}
	} while (!phys_reg && size > 0);

	/* If there is atleast one non-cached region, tag the
	 * whole region as IO region. This prevents systems 
	 * which donot like block reads to IO space (aka Everest)
	 * from panicing.
	 */
	if (phys_reg) {
		rp->r_flags |= RG_PHYSIO;
	}
	}

done:
#ifdef CKPT
	if (ckpt_enabled) {
		ASSERT(rp->r_ckptinfo == -1);
		rp->r_ckptflags = CKPT_MMAP;
		if (type == MAP_PRIVATE)
			rp->r_ckptflags |= CKPT_PRIVATE;
		rp->r_ckptinfo = arg->as_mmap_ckpt;
	}
#endif
	regrele(rp);
	mrunlock(&pas->pas_aspacelock);

	*ataddr = vaddr;
	return 0;
}


/*
 * add an exec mapping.
 * This is very similer to mmap except that:
 * 1) we (potentially) look for tsave mappings
 * 2) we handle zfod stuff
 * 3) no auto page locking
 */
#ifdef DEBUG
int execselfhit, execreatt_err;
#endif

int
pas_addexec(pas_t *pas, ppas_t *ppas, as_addspace_t *arg, uvaddr_t *ataddr)
{
	preg_t **pprp, *tmp_prp;
        struct pm *pm;
	int error = 0;
	mprot_t prot;
	struct region *rp;
	preg_t *prp;
	int pflags = 0, flags, type;
	ushort regflags = 0;
	uvaddr_t vaddr;
	vnode_t *vp;
	size_t zfodlen;
	off_t off;

	ASSERT(mrislocked_update(&pas->pas_aspacelock));

	flags = arg->as_exec_flags;
	type = flags & MAP_TYPE;
	prot = arg->as_prot;
	vaddr = arg->as_addr;
	vp = arg->as_exec_vp;
	off = arg->as_exec_off;

	ASSERT(poff(off) == 0);
	if (flags & MAP_AUTOGROW)
		regflags |= RG_AUTOGROW;
	if (flags & MAP_AUTORESRV)
		regflags |= RG_AUTORESRV;
	if (flags & MAP_TEXT)
		regflags |= RG_TEXT;
	if (flags & MAP_LOCAL)
		pflags |= PF_NOSHARE;
	if (flags & MAP_PRIMARY)
		pflags |= PF_PRIMARY; /* for /proc */

	if (type == MAP_PRIVATE) {
		regflags |= RG_CW | RG_ANON;
		pflags |= PF_DUP;
	}

	/*
	 * If mapping something SHARED and wants text,
	 * and we have saved pregions
	 * see if one of them fits perfectly
	 */
	if (pas->pas_tsave &&
	   ((flags & (MAP_SHARED|MAP_TEXT)) == (MAP_SHARED|MAP_TEXT)) &&
	   !(prot & PROT_W)) {
		/*
		 * If the section to be mapped matches a saved
		 * pregion, reclaim it...
		 * XXX protections, flags??
		 */
		for (pprp = &pas->pas_tsave; prp = *pprp;
				pprp = PREG_LINK_LVALUE(prp)) {
			if (prp->p_reg->r_vnode == vp &&
			    prp->p_regva == vaddr &&
			    prp->p_pglen == btoc(arg->as_length) &&
			    prp->p_reg->r_fileoff == arg->as_exec_off) {
#ifdef DEBUG
				execselfhit++;
#endif
				/*
				 * Don't remove the pregion from
				 * the tsave list as yet.
				 * This is so that if reattachpreg()
				 * fails, then the pregion is left 
				 * on the tsave list.
				 */
				tmp_prp = PREG_GET_NEXT_LINK(prp);
				pm = aspm_getdefaultpm(pas->pas_aspm, MEM_DATA);
				error = reattachpreg(pas, ppas, prp, pm);
				aspm_releasepm(pm);
				if (error == 0)
					*pprp = tmp_prp;
#ifdef DEBUG
				else
					execreatt_err++;
#endif
				*ataddr = vaddr;
				return error;
			}
		}
	}
	if ((rp = allocreg(vp, flags & MAP_SHARED ? RT_MAPFILE : RT_MEM,
						regflags)) == NULL)
		return ENOMEM;

	pm = aspm_getdefaultpm(pas->pas_aspm, MEM_DATA); 
	if (error = vattachreg(rp, pas, ppas, vaddr, (pgno_t)0, 0, PT_MAPFILE, 
			      prot, arg->as_maxprot, pflags, pm, &prp)) {
		aspm_releasepm(pm);
		freereg(pas, rp);
		return error;
	}
	aspm_releasepm(pm);
	
	rp->r_maxfsize = off + arg->as_length;
	rp->r_fileoff = off;

	if (error = vgrowreg(prp, pas, ppas, prot,
			btoc(arg->as_length + (vaddr - prp->p_regva)))) {
		vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen, 
			  RF_NOFLUSH);
		return error;
	}

	ASSERT(((prp->p_flags & PF_PRIVATE) != 0) ||
			(REG_REPLICABLE(prp->p_reg) == 0));

	zfodlen = arg->as_exec_zfodlen;
	if (zfodlen) {
		size_t pgleft;

		pgleft = poff((size_t)vaddr + arg->as_length);
		if (pgleft)
			pgleft = NBPP - pgleft;

		if (zfodlen > pgleft) {
			zfodlen -= pgleft;
			error = vgrowreg(prp, pas, ppas, PROT_RW, btoc(zfodlen));
			if (error) {
				(void) vdetachreg(prp, pas, ppas,
						prp->p_regva, 
						prp->p_pglen,
						RF_NOFLUSH);
				return error;
			}
		}
	}

#ifdef CKPT
	if (ckpt_enabled) {
		rp->r_ckptflags = CKPT_EXEC;
		rp->r_ckptinfo = arg->as_exec_ckpt;
	}
#endif
	regrele(rp);
	*ataddr = vaddr;

	/*
	 * if this should be considered for the heap start - do it
	 */
	if ((flags & MAP_BRK) && (vaddr >= pas->pas_brkbase)) {
		pas->pas_brkbase = vaddr + ctob(prp->p_pglen);
		ASSERT(poff(pas->pas_brkbase) == 0);
	}

	return 0;
}

/*
 * bring in a demand load section (one that can't be demand paged)
 */
int
pas_addload(pas_t *pas, ppas_t *ppas, as_addspace_t *arg, uvaddr_t *ataddr)
{
	reg_t *rp;
	struct pm *pm;
	mprot_t prot;
	preg_t *prp;
	int flags;
	uvaddr_t vaddr;
	vnode_t *vp;
	size_t zfodlen;
	int error;

	ASSERT(mrislocked_update(&pas->pas_aspacelock));
	flags = arg->as_load_flags;
	prot = arg->as_prot;
	vaddr = arg->as_addr;
	vp = arg->as_load_vp;

	rp = allocreg(NULL, RT_MEM, RG_ANON|RG_CW);

	/*
	 * Since this is a private mapping - set maxprots to RWX
	 */
	pm = aspm_getdefaultpm(pas->pas_aspm, MEM_DATA);
	error = vattachreg(rp, pas, ppas, vaddr, (pgno_t) 0, (pgno_t) 0,
		    PT_MEM, prot, PROT_ALL,
		    (flags & MAP_LOCAL) ? (PF_DUP|PF_NOSHARE) : PF_DUP,
		    pm, &prp);
	aspm_releasepm(pm);

	if (error) {
		freereg(pas, rp);
		return error;
	}

	/*
	 * Read in the segment in one big chunk
	 */
	if (arg->as_load_llength) {
		if (error = loadreg(prp, pas, ppas, arg->as_load_laddr,
				vp, arg->as_load_off, arg->as_load_llength)) {
			if (error == ENOMEM)
				(void)vdetachreg(prp, pas, ppas,
					prp->p_regva,
					prp->p_pglen, RF_NOFLUSH);
			else
				regrele(rp);
			return error;
		}
	}

	/*
	 * Now set protections.
	 */
	chgprot(pas, prp, prp->p_regva, prp->p_pglen, prot);

	/* handle any zfod */
	zfodlen = arg->as_load_zfodlen;
	if (zfodlen) {
		size_t pgleft;

		pgleft = poff((size_t)vaddr + arg->as_length);
		if (pgleft)
			pgleft = NBPP - pgleft;

		if (zfodlen > pgleft) {
			zfodlen -= pgleft;
			error = vgrowreg(prp, pas, ppas, PROT_RW, btoc(zfodlen));
			if (error) {
				(void) vdetachreg(prp, pas, ppas,
						prp->p_regva, 
						prp->p_pglen,
						RF_NOFLUSH);
				return error;
			}
		}
	}
#ifdef CKPT
	if (ckpt_enabled && (arg->as_load_llength)) {
		rp->r_ckptflags = CKPT_EXEC;
		rp->r_ckptinfo = arg->as_load_ckpt;
	}
#endif
	regrele(rp);
	*ataddr = vaddr;

	/*
	 * if this should be considered for the heap start - do it
	 */
	if ((flags & MAP_BRK) && (vaddr >= pas->pas_brkbase)) {
		pas->pas_brkbase = vaddr + ctob(prp->p_pglen);
		ASSERT(poff(pas->pas_brkbase) == 0);
	}
	return 0;
}

/*
 * Counter for color mismatches in pas_addprda. Color mismatches can affect
 * performance, so this counter is here to help diagnose performance 
 * problems.
 */
long prda_color_mismatch = 0;
/*
 * Add PRDA this consists of locking it in memory
 * and get a kernel address that points to it
 */
/* ARGSUSED */
int
pas_addprda(pas_t *pas, ppas_t *ppas, as_addspace_t *arg, uvaddr_t *ataddr)
{
	preg_t *prp;
	pde_t *pd;
	int error;
        pfd_t *pfd;

again:
	mraccess(&pas->pas_aspacelock);

	/*
	 * Create the prda now if we don't already have one.  The physical
	 * page will be allocated and initialized in vfault when lockpages
	 * tries to lock down the page.
	 */
	if ((prp = findpregtype(pas, ppas, PT_PRDA)) == NULL) {
		mrunlock(&pas->pas_aspacelock);
		mrupdate(&pas->pas_aspacelock);
		error = setup_prda(pas, ppas);
		mrunlock(&pas->pas_aspacelock);
		if (error)
			return error;
		goto again;

	}
	ASSERT(prp->p_reg->r_pgsz > 0);

	if (error = lockpages2(pas, ppas, prp->p_regva, prp->p_pglen, LPF_NONE)) {
		mrunlock(&pas->pas_aspacelock);
		return error;
	}
	if (prp->p_flags & PF_PRIVATE) {
		/* an sproc */
		ASSERT((ppas->ppas_flags & PPAS_LOCKPRDA) == 0);
		ppas_flagset(ppas, PPAS_LOCKPRDA);
	} else {
		/* a uthread?? */
		ASSERT((pas->pas_flags & PAS_LOCKPRDA) == 0);
		pas_flagset(pas, PAS_LOCKPRDA);
	}
	pd = pmap_pte(pas, prp->p_pmap, prp->p_regva, 0);
	ASSERT(pd != NULL && pg_isvalid(pd));
        pfd = pdetopfdat_hold(pd);
        /*
         * We must leave this pfdat in a held state
         * because we keep a flying reference to the
         * pfdat being used for the prda via p->p_prda.
         */
	mrunlock(&pas->pas_aspacelock);
	*ataddr = (uvaddr_t)page_mapin(pfd, VM_VACOLOR, colorof(PRDAADDR));
#ifdef USE_PTHREAD_RSA
	if (!IS_KSEG0(*ataddr) ||
	    (colorof(PRDAADDR) != pfncolorof(pfdattopfn(pfd)))) {
		prda_color_mismatch++;
#ifdef	DEBUG
		cmn_err(CE_WARN,
                        "Performance may be affected due to PRDA color mismatch, vmaddr 0x%x pfn 0x%x\n",
			*ataddr, pfdattopfn(pfd) );
#endif	/* DEBUG */
	}
#endif
	return(0);
}

/*
 * Select attach address.
 */

/*
 * Note that with RESERVE space below, this will normally carve a
 * much bigger space.
 */
#define HEAPMARGIN	(1024 * 1024 * 1024)  /* 1 Gb */
#if _MIPS_SIM == _ABI64
#define HEAPMARGIN64	0x2000000000	/* 1/8 Tb */
#endif
#define	SMARGIN	(128 * 1024 * 1024)	/* desired "breathing room" around */
					/* stack */
/*
 * Black book says that 0x30000000 through 0x3fff0000 is reserved
 * for third-party libs and shmem segs.  Just to make life simple,
 * we'll waste 64K and call it 0x40000000, a segment-aligned value.
 */
#define RESERVESTART	((uvaddr_t)0x30000000L)
#define RESERVEEND	((uvaddr_t)0x40000000L)

/*
 * Check attach address.
 */
static uvaddr_t
checkthisaddr(
	preg_set_t *pregset,
	uvaddr_t start,
	pgcnt_t npgs,
	uvaddr_t hiusrattach)
{
	preg_t	*prp;

	/*
	 * See if there are npgs available between start and hiusrattach.
	 * We ask the avl tree for the first (lowest addr) region that
	 * overlaps start,start+ctob(npgs); if there's no overlap, we're
	 * home, otherwise we walk the in-order list.
	 */
	prp = PREG_FINDANYRANGE(pregset,
			(__psunsigned_t) start,
			(__psunsigned_t) start + ctob(npgs),
			PREG_INCLUDE_ZEROLEN);

	if (!prp)
		return start;

	for ( ; ; ) {
		preg_t *nextprp, *tp;
		uvaddr_t end;
		size_t plen;

		plen = ctob(prp->p_pglen);
		if (!plen)
			plen = ctob(1);

		start = prp->p_regva + plen;
		end = start + ctob(npgs);

		if (end > hiusrattach)
			break;

		if (!(nextprp = PREG_NEXT(prp)))
			return start;

		/*
		 * See if we can skip past PRP_NEXT (avl_nextino),
		 * either through a superior parent or avl_next.
		 */
		if ((tp = (preg_t *)prp->p_avlnode.avl_parent) &&
		    tp != nextprp &&
		    tp->p_regva >= start) {
			/*
			 * We can skip to tp if regva '<=' end, instead of
			 * the logically-correct '<' because there _has_
			 * to be another pregion between prp and tp
			 * (i.e., nextprp) at this point, which would
			 * consume space.
			 */
			if (tp->p_regva <= end) {
				prp = tp;
				continue;
			}
		}

		if ((tp = (preg_t *)prp->p_avlnode.avl_forw) &&
		    tp != nextprp &&
		    tp->p_regva <= end) {
			prp = tp;
			continue;
		}

		if (nextprp->p_regva >= end)
			return start;

		prp = nextprp;
	}

	return 0;
}

static uvaddr_t
checkaddr(
	pas_t *pas,
	ppas_t *ppas,
	uvaddr_t start,
	pgcnt_t npgs,
	uvaddr_t hiusrattach)
{
	uvaddr_t tstart = 0;
	uvaddr_t end = start + ctob(npgs);

	while (end <= hiusrattach) {

		/*
		 * checkthisaddr will return the address of the
		 * first space between start and hiusrattach
		 * owned by pregset that has npgs available.
		 * Zero implies failure.
		 */
		if (!(start = checkthisaddr(&pas->pas_pregions,
						start, npgs, hiusrattach)))
			break;

		/*
		 * ppas usually has no pregions attached -- test this
		 * before jumping down several subroutines
		 */
		if (PREG_FIRST(ppas->ppas_pregions)) {
			tstart = checkthisaddr(&ppas->ppas_pregions,
						start, npgs, hiusrattach);
			if (start != tstart) {
				start = tstart;
				goto next;
			}
		}

		end = start + ctob(npgs);
		ASSERT(end <= hiusrattach);

		/*
		 * The black book says we shouldn't put regions in
		 * the reserved range, at least when placing by default.
		 */
		if ((start >= RESERVESTART && start < RESERVEEND) ||
		    (start <= RESERVESTART && end > RESERVESTART)) {
			start = (uvaddr_t)RESERVEEND;
			goto next;
		}

		/*
		 * If this is a shared-address process, check for
		 * overlaps with any address brethern.
		 */
		if (pas->pas_flags & PAS_SHARED) {
			ppas_t *tppas;

			/*
			 * Check that the address under consideration
			 * doesn't overlap any others' private mapping.
			 * We have address space locked, so this list
			 * isn't going anywhere.
			 */
			for (tppas = pas->pas_plist;
			     tppas;
			     tppas = tppas->ppas_next) {
				if ((tppas == ppas) ||
				    !(tppas->ppas_flags & PPAS_SADDR))
					continue;

				tstart = checkthisaddr(&tppas->ppas_pregions,
							start, npgs,
							hiusrattach);
				if (start != tstart) {
					start = tstart;
					goto next;
				}
			}
		}

		/*
		 * all through w/o an overlap - it's a glorious day
		 */
		return start;
next:
		if (!start)
			break;
		end = start + ctob(npgs);
	}

	return 0;
}

static uvaddr_t checkaddr_push(pas_t *, ppas_t *, uvaddr_t, uvaddr_t);
static uvaddr_t checkaddr_adjust(pas_t *, ppas_t *,
				uvaddr_t, uvaddr_t, pgcnt_t, uvaddr_t);

uvaddr_t
pas_allocaddr(pas_t *pas, ppas_t *ppas, pgcnt_t npages, uchar_t color)
{
	uvaddr_t start, s2;
	uvaddr_t start_save = NULL;
	uvaddr_t lousrattach, hiusrattach;
	uvaddr_t brkbase, maginot, stackbase, smargin;
	preg_t *prp;
	pgcnt_t	npgs = npages;

	ASSERT(mrislocked_update(&pas->pas_aspacelock));

	if (color != NOCOLOR)
		npgs += cachecolormask;

	/*
	 * First thing we'll do is scope out the VM layout of the caller.
	 * The typical process has some amount of space in front of the
	 * text/break area, and has a hugh chunk of space bewteen the
	 * heap and the stack.

	 lousrattach	code... data(break)	maginot	    stack/hiusrattach
	     |	 1->		|		<-3|2->		|

	 32bit:

	 0x00000000
			0x400000 lousrattach
			system libs
	 0x10000000	text
			data
			brkbase somewhere past data
	 0x20000000

	 0x30000000	RESERVESTART
			   |	heapmargin hereabouts
	 0x40000000	RESERVEEND

	 0x50000000

	 0x60000000
			maginot hereabouts

	 0x70000000
			smargin
			stkbase
	 0x80000000

	 * We'll first try to allocate in the space between lousrattach
	 * and the heap, then between the maginot and the stack (actually,
	 * smargin), the then between maginot and RESERVESEND, and then
	 * between RESERVESTART and the heap (assuming the heap hasn't
	 * already grown there).
	 *
	 * To calculate the maginot, we take the current heap and add
	 * HEAPMARGIN, then take the current stack and subtract SMARGIN,
	 * then find the halfway point.
	 *
	 * N64 allocations are a lot easier.  The text and data spaces
	 * are usually down near 0x10000000, and if there is at least
	 * HEAPMARGIN64 bytes between the current heap and LOWDEFATTACH64,
	 * we just start at HIUSRATTACH_64 and go forward.  Since
	 * HEAPMARGIN64 is 1/8 TB, LOWDEFATTACH64 is at 1/4 TB, and
	 * HIDEFATTACH64 is at 1 TB, that leaves _lots_ of space for
	 * heap growth (up to LOWDEFATTACH64) and allocating maps.
	 */

#if _MIPS_SIM == _ABI64
	if (pas->pas_flags & PAS_64) {
		lousrattach = LOWDEFATTACH64;
		hiusrattach = (uvaddr_t)HIUSRATTACH_64;
	} else
#endif
	{
		lousrattach = LOWDEFATTACH;
		hiusrattach = (uvaddr_t)HIUSRATTACH_32;
	}

	if (prp = findpreg(pas, ppas, pas->pas_brkbase))
		brkbase = prp->p_regva + ctob(prp->p_pglen);
	else {
#if _MIPS_SIM == _ABI64
		if (pas->pas_flags & PAS_64) {
			brkbase = pas->pas_brkbase ? pas->pas_brkbase :
							LOWDEFATTACH64;
		} else
#endif
		brkbase = MAX(lousrattach, pas->pas_brkbase);
	}

	/*
	 * Do a little sanity checking before setting stackbase.
	 */
	if ((prp = findpreg(pas, ppas, pas->pas_lastspbase)) &&
	    (prp->p_regva > brkbase))
		stackbase = prp->p_regva;
	else
		stackbase = hiusrattach;

	smargin = stackbase - SMARGIN;

	/*
	 * Center maginot to be halfway between the heap (plus margin)
	 * and the stack (plus margin) for 32-bit programs.  For 64-bit,
	 * LOWDEFATTACH64 is fine if it leaves plenty of heap margin.
	 * [Note that for 64-bit programs, LOWDEFATTACH64 is well above
	 * the normal text and data load addresses.]
	 */
#if _MIPS_SIM == _ABI64
	if ((pas->pas_flags & PAS_64) &&
	    (LOWDEFATTACH64 >= brkbase + HEAPMARGIN64))
		maginot = (uvaddr_t) LOWDEFATTACH64;
	else
#endif
	{
	maginot = (uvaddr_t) (((__psunsigned_t)brkbase + HEAPMARGIN) / 2 +
			      ((__psunsigned_t)smargin) / 2);

	if (maginot < RESERVEEND)
		maginot = RESERVEEND;
	}
	if (maginot > smargin)
		maginot = smargin;
	/*
	 * round to a segment to conserve page tables
	 */
	maginot = (uvaddr_t)stob(btost(maginot));

	/*
	 * Try to allocate just past last allocation.
	 */
	if ((start = pas->pas_nextaalloc) && (start + ctob(npgs) <= smargin)) {
		if (start < brkbase || start >= maginot) {
			if ((start + ctob(npgs) > start) && /* overflow check */
			    (start = checkaddr(pas, ppas,
					      start, npgs, start + ctob(npgs))))
			{
				goto got_it;
			}
		}

		/*
		 * We don't know for sure if pas_nextaalloc got set by
		 * this routine, in which case it is the end address of
		 * the (probably) empty spot in the address space, or
		 * because of an unmap call, in which case it is the
		 * start of the empty address space.  So decrement start,
		 * which is necessary if it is the end, but double the
		 * search size in case it was the latter.  We then call
		 * checkaddr_push to adjust it upwards as far as possible.
		 */
		if (start >= RESERVEEND && start < maginot) {
			start -= ctob(npgs);
			if (start < RESERVEEND)
				start = RESERVEEND;
			s2 = start + ctob(npgs)*2;
			if ((start < s2) &&	/* overflow check */
			    (start = checkaddr(pas, ppas, start, npgs, s2))) {
				start = checkaddr_push(pas, ppas,
						       start + ctob(npgs), s2)
						- ctob(npgs);
				goto got_it;
			}
		}
	}

	/*
	 * Try to allocate room before the data (heap) region,
	 * being careful not to step on any reserved areas.
	 */
	if ((lousrattach + ctob(npgs) <= brkbase) &&
	    (start = checkaddr(pas, ppas, lousrattach, npgs, brkbase))) {
		goto got_it;
	}

	/*
	 * Couldn't find room in the space in front of the heap.
	 * Now try in the space between heap+HEAPMARGIN and the stack.
	 * If the returned address is too close to the stack, remember
	 * it because it might be our only option.
	 */
	s2 = maginot + ctob(npgs);
	if ((s2 > maginot) &&	/* overflow? */
	    (s2 <= stackbase) &&
	    (start = checkaddr(pas, ppas, maginot, npgs, stackbase)))
	{
		if (start + ctob(npgs) <= smargin)
			goto got_it;
		else
			start_save = start;
	}

	/*
	 * Ok -- couldn't find enough space in front of the heap,
	 * or between heap+HEAPMARGIN and stackbase-STACKMARGIN.
	 *
	 * We know that RESERVEEND is closer to the heap than maginot.
	 * Let's see if there's any free spot between RESERVEEND and
	 * the stack.  But we don't need to look beyond 'maginot+ctob(npgs)',
	 * because we've already looked there.
	 * We _do_ need to look through 'maginot+ctob(npgs-1)', though.
	 * Remember that s2 now points to maginot + ctob(npgs).
	 */
	if (s2 > stackbase || s2 < maginot)
		s2 = stackbase;

	/*
	 * There's no need to check for overflow in
	 * (RESERVEEND + ctob(npgs) -- checkaddr will catch it.
	 */
	if ((RESERVEEND + ctob(npgs) <= s2) &&
	    (start = checkaddr(pas, ppas, RESERVEEND, npgs, s2)))
	{
		if (start + ctob(npgs) <= smargin) {
			start = checkaddr_push(pas, ppas,
					start + ctob(npgs), smargin)
						- ctob(npgs);
			goto got_it;
		} else {
			/*
			 * Is start better than start_save?
			 * Yes, as long as start is further from the
			 * heap than start_save is from the stack.
			 */
			if ((!start_save) ||
			    (start - brkbase) >
			    (stackbase - (start_save + ctob(npgs))))
				start_save = start;
		}
	}

	/*
	 * If we have an address already but it's too close to either
	 * the heap or stack, call checkaddr_adjust to make any
	 * adjustments it can to make the allocation less annoying.
	 * Note that checkaddr_adjust won't return a space closer
	 * than SMARGIN bytes from stackbase unless it has no choice.
	 */
	if (start_save)
		start_save = checkaddr_adjust(pas, ppas,
					brkbase, start_save, npgs, stackbase);

	/*
	 * We might have an address space between RESERVEEND and
	 * the stack, but it extends beyond smargin.  So let's look
	 * in front of RESERVEEND, and see if we can find any space
	 * there.  If so, remember the highest address space.
	 */
	start = NULL;
	s2 = brkbase;
	while ((s2 + ctob(npgs) <= RESERVESTART) &&
	       (s2 = checkaddr(pas, ppas, s2, npgs, RESERVESTART)))
	{
		/*
		 * s2 now points to the beginning of a free space...
		 */

		s2 = checkaddr_push(pas, ppas, s2 + ctob(npgs), RESERVESTART);

		/*
		 * and s2 now points either to RESERVESTART or to
		 * the end of the free space.
		 */
		start = s2 - ctob(npgs);
	}

	/*
	 * If we found anything between brkbase and RESERVESTART,
	 * return it if there was nothing beyond RESERVEEND.
	 * Otherwise, return the space that offends the break or
	 * stack spaces least.
	 */
	if (start) {
		ASSERT(start >= brkbase);
		if (start_save &&
		    (start - brkbase <= stackbase - (start_save + ctob(npgs))))
			start = start_save;
		goto got_it;
	}

	if (!(start = start_save))
		return 0;

got_it:
	if (color != NOCOLOR) {
		__psunsigned_t addr = (__psunsigned_t)btoct(start);

		color &= cachecolormask;
		while ((addr & cachecolormask) != color)
			addr++;
		start = (uvaddr_t)ctob(addr);
	}

	if (start < brkbase || start >= maginot)
		pas->pas_nextaalloc = start + ctob(npages);
	else
		pas->pas_nextaalloc = start;

	return start;
}

/*
 * Check whether we can grow into the address space start-end without
 * overlapping any of the process address space.  The caller has an
 * address space offered, but is interested in pushing the space up
 * as much as possible to get out of the way of the heap.
 */
static uvaddr_t
checkaddr_push(
	pas_t *pas,
	ppas_t *ppas,
	uvaddr_t start,
	uvaddr_t end)
{
	preg_t	*prp;
	uvaddr_t rend = end;

	prp = PREG_FINDANYRANGE(&pas->pas_pregions,
			(__psunsigned_t) start,
			(__psunsigned_t) rend,
			PREG_INCLUDE_ZEROLEN);

	if (prp) {
		if ((rend = prp->p_regva) == start)
			return rend;
	}

	/*
	 * ppas usually has no pregions attached -- test this
	 * before jumping down several subroutines
	 */
	if (PREG_FIRST(ppas->ppas_pregions)) {
		if (prp = PREG_FINDANYRANGE(&ppas->ppas_pregions,
				(__psunsigned_t) start,
				(__psunsigned_t) rend,
				PREG_INCLUDE_ZEROLEN))
		{
			if ((rend = prp->p_regva) == start)
				return rend;
		}
	}

	/*
	 * If this is a shared-address process, check for
	 * overlaps with any address brethern.
	 */
	if (pas->pas_flags & PAS_SHARED) {
		ppas_t *tppas;

		/*
		 * Check that the address under consideration
		 * doesn't overlap any others' private mapping.
		 * We have address space locked, so this list
		 * isn't going anywhere.
		 */
		for (tppas = pas->pas_plist;
		     tppas;
		     tppas = tppas->ppas_next) {
			if ((tppas == ppas) ||
			    !(tppas->ppas_flags & PPAS_SADDR))
				continue;

			if (prp = PREG_FINDANYRANGE(&tppas->ppas_pregions,
					(__psunsigned_t) start,
					(__psunsigned_t) rend,
					PREG_INCLUDE_ZEROLEN))
			{
				if ((rend = prp->p_regva) == start)
					break;
			}
		}
	}

	return rend;
}

/*
 * Caller has a free space that annoys either the break area or the
 * stack.  Adjust it such it annoys them equally (if possible).
 */
static uvaddr_t
checkaddr_adjust(
	pas_t *pas,
	ppas_t *ppas,
	uvaddr_t brkbase,
	uvaddr_t start,
	pgcnt_t npgs,
	uvaddr_t stackbase)
{
	uvaddr_t end;
	pgcnt_t size;
	long before, after;

	if ((end = start + ctob(npgs)) < stackbase)
	{
		end = checkaddr_push(pas, ppas, end, stackbase);
		ASSERT(end >= start + ctob(npgs));

		/*
		 * Ok -- we've got start to end reserved.
		 * Wiggle the addresses to offend the heap and stack
		 * equally, or until we're got at least HEAPMARGIN
		 * space from the heap and at least SMARGIN bytes
		 * for stack growth.
		 */
		size = btoct(end - start);
		size -= npgs;

		before = start - brkbase;
		after = stackbase - (start + ctob(npgs));

		while (size &&
		       ((before < after) ||
			(before < HEAPMARGIN && after > SMARGIN)))
		{
			size--;
			before += ctob(1);
			after -= ctob(1);
			start += ctob(1);
		}
	}

	return start;
}

/*
 * Build the user's stack.
 * args is a kernel virtual address to be used to
 * initialize the stack with the exec arguments
 */
/* ARGSUSED3 */
int
pas_addstackexec(pas_t *pas, ppas_t *ppas, as_addspace_t *arg, uvaddr_t *ataddr)
{
	reg_t *rp;
	auto preg_t  *prp;
	pgcnt_t	nargc;
	pde_t *pt;
	char *lsp;			/* local stack pointer */
	pgno_t apn;			/* anon page number */
	auto pgno_t sz;
	int error;
        pfd_t *pfd;
        struct pm *pm;
	uvaddr_t newsp;
	pde_t *stkptp;

	ASSERT(mrislocked_update(&pas->pas_aspacelock));
	/* stacks grow down .. */
	newsp = arg->as_addr - arg->as_length;
	stkptp = kvtokptbl(arg->as_stackexec_contents) - 1;

	pas->pas_stksize = arg->as_length;
	nargc = btoc(pas->pas_stksize);

	/*
	 * kind of a hack - we know that we are thes last phase of exec,
	 * so check for exceeding of vmemmax here.
	 */
	if (chkpgrowth(pas, ppas, nargc))
		return ENOMEM;

	if (VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND, 0, nargc)) {
		miser_inform(MISER_KERN_MEMORY_OVERRUN);
		return ENOMEM;
	}

	rp = allocreg(NULL, RT_MEM, RG_ANON);

	/*
	 * stack region is attached first at top of of stack - then grown
	 * Permit mprotect setting stack executable by using a maxprot of RWX
	 */
        pm = aspm_getdefaultpm(pas->pas_aspm, MEM_STACK);
	if (error = vattachreg(rp, pas, ppas, arg->as_addr,
			      (pgno_t) 0, rp->r_pgsz,
			      PT_STACK, PROT_RW, PROT_RWX, PF_DUP, pm, &prp)) {
                aspm_releasepm(pm);
		freereg(pas, rp);
		VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND, 
					0, -nargc);
		return error;
	}

        /*
         * We are using this pm later to do the rmaps.
         * We can release it now because we have a reference
         * from the region we are growing.
         */
        aspm_releasepm(pm);

	/*
	 * we use r_maxfsize to hold hint as to how much margin to
	 * give when looking to add a region near this one
	 * Note - this is only called by exec so we know we're
	 * not part of a share group so the pas_stkmax is always the correct
	 * one to use.  Ditto for pas_stksize above and pas_stkbase below.
	 */
	rp->r_maxfsize = ctob(btoc64(pas->pas_stkmax));
	
	/*
	 * Grow the stack but don't actually allocate any pages.
	 */
	if (error = vgrowreg(prp, pas, ppas, PROT_RW, nargc)) {
		(void) vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen,
				 RF_NOFLUSH);
		VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND, 
					0, -nargc);
		return error;
	}
	pas->pas_stkbase = prp->p_regva;

	/*
	 * Initialize the stack with the pages containing the
	 * exec arguments.  We got these from exece().
	 *
	 * stack grows down, need to copy highest pages
	 * that kernel init'd.
	 * These pde's go at the HIGH-END of the first page of
	 * page tables, starting at the page below userstack.
	 */
	lsp = (char *)newsp;
	apn = vtoapn(prp, lsp);
	prp->p_nvalid = nargc;
	stkptp -= nargc - 1;
	while (nargc) {
		ASSERT(nargc > 0);
		ASSERT(lsp >= prp->p_regva);
		sz = nargc;
		pt = pmap_ptes(pas, prp->p_pmap, lsp, &sz, 0);
		ASSERT(sz);
		nargc -= sz;
		for ( ; --sz >= 0; pt++, stkptp++, lsp += NBPP, apn--) {
			pgno_t pfn;

                        pfd = pdetopfdat(stkptp);
			pfd->pf_flags &= ~P_DUMP;
			pfn = pfdattopfn(pfd);
                        pg_setpfn(pt, pfn);
                        anon_insert(rp->r_anon, pfd, apn, P_DONE);
			pg_setvalid(pt);
			pg_setmod(pt);
			pg_setcachable(pt);

                        /*
                         * It's safe to use the pm here because
                         * we have a reference from the region we
                         * are growing.
                         */

			VPAG_UPDATE_RMAP_ADDMAP(PAS_TO_VPAGG(pas), 
				JOBRSS_INS_PFD, pfd, pt, pm);

#if R4000 || R10000
#ifdef MH_R10000_SPECULATION_WAR
                        if (IS_R10000()) {
                                __psint_t vfn = kvirptetovfn(stkptp);

                                krpf_remove_reference(pfntopfdat(pfn),
                                                      vfn);
                                pg_setpgi(stkptp, mkpde(PG_G, 0));
                                unmaptlb(0,vfn);
                        } else
#endif /* MH_R10000_SPECULATION_WAR */
			pg_setpgi(stkptp, mkpde(PG_G, 0));
#else
			pg_clrpgi(stkptp);
#endif
		}
	}

	if (poff(newsp)) {
		pt = pmap_pte(pas, prp->p_pmap, (char *)newsp, 0);
		ASSERT(pt);

		/* We need to use page_zero rather than just invoking bzero since
		 * we can't allow a tfault to occur (our invoker may have
		 * aspacelock and tfault will hang attempting to obtain the same
		 * lock.
		 * On SN0, use page_zero to take advantage of BTE.
		 * Now used on all platforms since kernel preemption and process
		 * migration means the tlbpid() can change on us while we're
		 * trying to perform a tlbdropin,
		 * so we simplify and use common code.
		 */
		/* zero from end of stack to page boundary */
		pfd = pdetopfdat_hold(pt); 
		page_zero(pfd, colorof(newsp), 0, poff(newsp));
		pfdat_release(pfd);
	}

	/*
	 * We keep region locked until now so that vhand does not steal
	 * page while we're performing a bzero, which would cause a vfault
	 * and a double trip on the aspacelock.
	 */
	regrele(rp);
	return 0;
}

/*
 * pas_addattach - handle prctl(ATTACHADDR)
 */
int
pas_addattach(
	pas_t *pas,
	ppas_t *ppas,
	as_addspace_t *asd,
	as_addspaceres_t *asres)
{
	preg_t *prp;
	reg_t *rp;
	uvaddr_t vaddr;
	auto preg_t *nprp;
	int pflags = PF_DUP;
	vasid_t sourcevasid;
	ppas_t *sourceppas;
	mprot_t prot;
	int error;
	struct pm *pm;
	int differentpas = 0;
	pas_t *sourcepas;

	if (asd->as_attach_flags & AS_ATTACH_MYSELF) {
		sourceppas = ppas;
	} else {
		/* check that asid is in our address space */
		if (as_lookup(asd->as_attach_asid, &sourcevasid)) {
			return ENOENT;
		}
		if ((sourcepas = VASID_TO_PAS(sourcevasid)) != pas) {
			/* asid might still be in our share group even if
			 * the pas' are different via "sproc(,NULL,)"
			 */
			if (!isshdmember(asd->as_attach_asid)) {
				as_rele(sourcevasid);
				return ESRCH;
			}
			differentpas = 1;
		}
		sourceppas = (ppas_t *)sourcevasid.vas_pasid;
	}

	/*
	 * If attaching across address spaces, find and lock the region
	 * from the source AS.  This comes before the mrupdate of the
	 * destination AS to avoid possible deadlocks.
	 */
	if (differentpas) {
		mraccess(&sourcepas->pas_aspacelock);
		if ((prp = findpreg(sourcepas, sourceppas, 
				    asd->as_attach_srcaddr)) == NULL) {
			mrunlock(&sourcepas->pas_aspacelock);
			as_rele(sourcevasid);
			return ENOENT;
		}
		reglock(prp->p_reg);
		mrunlock(&sourcepas->pas_aspacelock);
		as_rele(sourcevasid);
	}

	mrupdate(&pas->pas_aspacelock);

	if (!differentpas) {
		/* since we have aspacelock excl not much can happen, so
		 * its safe to release the address space
		 */
		if (sourceppas != ppas)
			as_rele(sourcevasid);

		/*
		 * look up address in source address space
		 */
		if ((prp = findpreg(pas, sourceppas, asd->as_attach_srcaddr)) == NULL) {
			mrunlock(&pas->pas_aspacelock);
			return ENOENT;
		}
		reglock(prp->p_reg);
	}

	rp = prp->p_reg;

	/*
	 * If caller didn't specify local attach address, then
	 * go pick an address.
	 */
	if (asd->as_addr == AS_ADD_UNKVADDR) {
		if ((vaddr = pas_allocaddr(pas, ppas, prp->p_pglen,
					rp->r_color)) == NULL) {
			regrele(rp);
			mrunlock(&pas->pas_aspacelock);
			return ENOMEM;
		}
	} else
		vaddr = asd->as_addr;

	/*
	 * these attached regions will be inherited in future
	 * sprocs
	 * XXX would like to extend attr_dup so that it
	 * can dup and chg vaddr ... For now just let them RW
	 */
	prot = asd->as_prot;
	if (prot) {
		/* Caller specified desired permissions.  Make sure
		 * user is entitled to what's been requested.
		 */
		if ((prot & prp->p_maxprots) != prot) {
			regrele(prp->p_reg);
			mrunlock(&pas->pas_aspacelock);
			return EACCES;
		}
	} else {
		prot = PROT_RW & prp->p_maxprots;
	}

	/* If attaching to stack region need to preserve region
	 * type so that apn's get allocated consistently between
	 * the two different views of the region.
	 */
	pm = aspm_getdefaultpm(pas->pas_aspm,
			prp->p_type == PT_STACK ? MEM_STACK : MEM_DATA);
	error = vattachreg(prp->p_reg, pas, ppas, vaddr,
			prp->p_offset,
			prp->p_pglen,
			(prp->p_type == PT_STACK ? PT_STACK :PT_MEM),
			prot,
			prp->p_maxprots, pflags, pm, &nprp);
	aspm_releasepm(pm);
#ifdef R4000
	newptes(pas, ppas, 1);
#endif

	/* return same logical offset into region as user passed in */
	asres->as_addr = vaddr;
	asres->as_attachres_vaddr = ((asd->as_attach_srcaddr - prp->p_regva)
					    + vaddr);
	asres->as_attachres_size = NBPC * prp->p_pglen;
	asres->as_prot = prot;
	regrele(prp->p_reg);
	mrunlock(&pas->pas_aspacelock);
	return error;
}

int
pas_addshm(
	pas_t *pas,
	ppas_t *ppas,
	as_addspace_t *asd,
	uvaddr_t *ataddr)
{
	preg_t *prp;
	reg_t *rp;
	uvaddr_t vaddr;
	size_t size;
	int error = 0;
	struct pm *pm;
	boolean_t shattr = B_FALSE;	/* SPT */

	size = asd->as_length;
	vaddr = asd->as_addr;
	rp = asd->as_shm_mo.as_mohandle;

	mrupdate(&pas->pas_aspacelock);
	if (pas_getnshm(pas, ppas) >= asd->as_shm_maxsegs) {
		error = EMFILE;
		goto at_out;
	}

	if (vaddr == 0) {
		vaddr = pas_allocaddr(pas, ppas, btoc(size), rp->r_color);
		if (vaddr == NULL) {
			error = ENOMEM;
			goto at_out;
		}
	}
	reglock(rp);

#ifdef _VCE_AVOIDANCE
	if (vce_avoidance &&
	    rp->r_color != NOCOLOR &&
	    colorof(vaddr) != rp->r_color) {
		regrele(rp);
		error = EINVAL;
		goto at_out;
	}
#endif /* _VCE_AVOIDANCE */

	if (rp->r_pgsz && chkpgrowth(pas, ppas, rp->r_pgsz) < 0) {
		regrele(rp);
		error = ENOMEM;
		goto at_out;
	}

        pm = aspm_getdefaultpm(pas->pas_aspm, MEM_DATA);
	if (error = vattachreg(rp, pas, ppas, vaddr, (pgno_t)0, rp->r_pgsz,
				PT_SHMEM,
				asd->as_prot, asd->as_maxprot, 0, pm, &prp)) {
                aspm_releasepm(pm);
		regrele(rp);
		goto at_out;
	}
        aspm_releasepm(pm);

#ifdef _VCE_AVOIDANCE
	if (vce_avoidance && rp->r_color == NOCOLOR)
		rp->r_color = colorof(vaddr);
#endif /* _VCE_AVOIDANCE */

	/*
	 * SPT
	 * Try to get shared PT descriptor (see comment in pmap.c)
	 * Miser jobs are prevented so that we can track their rss.
	 */
	if ((asd->as_shm_flags & AS_SHM_SHATTR) 
					&& (PAS_TO_VPAGG(pas) == NULL)) {
		if (error = pmap_spt_get(pas, (pmap_sptid_t)rp, prp->p_pmap,
					 vaddr, size)) {
			vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen,
				  RF_NOFLUSH);
			goto at_out;
		}
		shattr = B_TRUE;
	}

	if (asd->as_shm_flags & AS_SHM_INIT) {
		if (chkpgrowth(pas, ppas, btoc(size)) < 0) {
			vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen,
				  RF_NOFLUSH);
			error = ENOMEM;
			goto at_out;
		}
		if ((error = vgrowreg(prp, pas, ppas, asd->as_prot, btoc(size))) != 0) {
			vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen,
				  RF_NOFLUSH);
			goto at_out;
		}
	} else if (shattr == B_FALSE) {		/* SPT: avoid pre-faulting */
		attr_t  *attr;
		void *id;
		pgcnt_t npgs;
		pgno_t rpn, apn;
		pde_t *pt;
		pfd_t *pfd;
		int    nvalid = 0;

#ifdef R4000
		newptes(pas, ppas, 0);
#endif

		/*
		 * Load the pmap with any pages that are already in memory.
		 * This saves a lot of vfault overhead for big shmem regions
		 * that are shared by many processes.  Since shmem is known
		 * to a simple anon region (no autogrow, autoreserve, or file
		 * store), we can do things more simply here than in the general
		 * vfault code plus save a lot of traps.
		 */
		vaddr = prp->p_regva;
		rpn = vtorpn(prp, vaddr);
		npgs = prp->p_pglen;
		attr = findattr(prp, vaddr);

		/*
		 * If the address space range consists of large pages then
		 * don't fault them in now. The same thing holds for
 		 * miser jobs. Miser jobs have to go through RSS limit checks.
		 * Let the fault handler do the RSS limit checks.
		 */
		if (PMAT_GET_PAGESIZE(attr) > NBPP || PAS_TO_VPAGG(pas))
			npgs = 0;

		for (; npgs; rpn++, vaddr += NBPP, npgs--) {
		    sm_swaphandle_t onswap;
		  
		    apn = rpntoapn(prp, rpn);
    
		    /* any reason not to sleep here??? */
		    if ((pt = pmap_pte(pas, prp->p_pmap, vaddr, VM_NOSLEEP)) == NULL)
			    continue;

		    /* see if physical page frame is already allocated */
		    if ((pfd = anon_pfind(rp->r_anon, apn, &id, &onswap)) ==
			 NULL)
			    continue;
    
		    /* i/o could be in-progress for this page */
		    if (!(pfd->pf_flags & P_DONE)) {
			    /* pfind(VM_ATTACH) bumps up pf_use, decrement */
			    pagefreesanon(pfd, 0);
			    continue;
		    }

		    /* on the R4X00PCs, the primary cache may need flushing */
		    COLOR_VALIDATION(pfd, colorof(vaddr), 0, VM_NOSLEEP);

		    pt->pte.pg_pfn = pfdattopfn(pfd);
		    pg_setccuc(pt, attr->attr_cc, attr->attr_uc);
		    pg_setvalid(pt);

		    nvalid++;
                    /*
		     * No real job rss update, miser jobs not allowed here.
		     */
		    VPAG_UPDATE_RMAP_ADDMAP(PAS_TO_VPAGG(pas), JOBRSS_INS_PFD,
				pfd, pt, attr_pm_get(attr));
		}
		prp->p_nvalid += nvalid;
	}

	/*
	 * SPT
	 * Attach to shared PT's (see comment in pmap.c)
	 */
	if (shattr &&
	    pmap_spt_attach((pmap_sptid_t)rp, pas, prp->p_pmap, vaddr))
		cmn_err(CE_PANIC, "shmat: shared attribute, attach failed");

	*ataddr = prp->p_regva;

	regrele(rp);
at_out:
	mrunlock(&pas->pas_aspacelock);
	return error;
}

int
pas_addinit(
	pas_t *pas,
	ppas_t *ppas,
	as_addspace_t *asd,
	uvaddr_t *ataddr)
{
	auto preg_t	*prp;
	register reg_t	*rp;
	struct pm       *pm;
	pgcnt_t	npgs;

	mrupdate(&pas->pas_aspacelock);
	rp = allocreg(NULL, RT_MEM, RG_ANON);
	pm = aspm_getdefaultpm(pas->pas_aspm, MEM_TEXT);
	if (vattachreg(rp, pas, ppas, asd->as_addr, (pgno_t) 0, 
		      rp->r_pgsz, PT_MEM, asd->as_prot, asd->as_maxprot,
		      PF_DUP, pm, &prp) != 0) {
		aspm_releasepm(pm);
		cmn_err(CE_PANIC, "attachreg: icode");
	}
	aspm_releasepm(pm);

	npgs = btoc(asd->as_length);
	if (vgrowreg(prp, pas, ppas, asd->as_prot, npgs) != 0)
		cmn_err(CE_PANIC, "growreg: icode");
	regrele(rp);
	mrunlock(&pas->pas_aspacelock);

	*ataddr = asd->as_addr;

	return 0;
}

/*
 * determine if address space is include in current process' share group via
 * shared group descriptor's process link - see PV 643340
 */
static int
isshdmember(asid_t asid)
{
	register int found = 0;
	register shaddr_t *sa = curprocp->p_shaddr;
	register proc_t *p;
	register uthread_t *ut;

	mutex_lock(&sa->s_listlock, 0);
	for (p = sa->s_plink; p; p = p->p_slink) {
		ut = prxy_to_thread(&p->p_proxy);
		if (ut->ut_asid.as_pasid == asid.as_pasid) {
			found++;
			break;
		}
	}

	mutex_unlock(&sa->s_listlock);
	return found;
}
