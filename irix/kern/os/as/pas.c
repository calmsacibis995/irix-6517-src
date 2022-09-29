/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident  "$Revision: 1.59 $"

#include "sys/types.h"
#include "sys/anon.h"
#include "ksys/as.h"
#include "as_private.h"
#include "sys/atomic_ops.h"
#include "sys/cachectl.h"
#include "sys/cmn_err.h"
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
#include <ksys/vpag.h>
#include <ksys/vm_pool.h>

/*
 * local AS ops
 */
/* ARGSUSED */
void
pas_lock(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	as_lockop_t op)		/* READ/EXCL */
{
	pas_t *pas = BHV_TO_PAS(bhv);

	if (op == AS_SHARED)
		mraccess(&pas->pas_aspacelock);
	else
		mrupdate(&pas->pas_aspacelock);
}

/*
 * asvo_trylock - return 1 if could get lock
 */
/* ARGSUSED */
int
pas_trylock(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	as_lockop_t op)		/* READ/EXCL */
{
	pas_t *pas = BHV_TO_PAS(bhv);
	int rv;

	if (op == AS_SHARED)
		rv = mrtryaccess(&pas->pas_aspacelock);
	else
		rv = mrtryupdate(&pas->pas_aspacelock);
	return rv;
}

/* ARGSUSED */
int
pas_islocked(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	as_lockop_t op)		/* SHARED/EXCL */
{
	pas_t *pas = BHV_TO_PAS(bhv);
	int rv;

	if (op == AS_SHARED)
		rv = mrislocked_access(&pas->pas_aspacelock);
	else
		rv = mrislocked_update(&pas->pas_aspacelock);
	return rv;
}

/* ARGSUSED */
void
pas_unlock(
	bhv_desc_t *bhv,
	aspasid_t pasid)	/* private address space id */
{
	pas_t *pas = BHV_TO_PAS(bhv);

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));
	mrunlock(&pas->pas_aspacelock);
}

int
pas_fault(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	as_faultop_t op,
	as_fault_t *as,		/* input parameters */
	as_faultres_t *asres)	/* return info */
{
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;
	int error;

	switch (op) {
	case AS_VFAULT:
		error = pas_vfault(pas, ppas, as, asres);
		break;
	case AS_PFAULT:
		error = pas_pfault(pas, ppas, as, asres);
		break;
	default:
		panic("pas_fault");
		/* NOTREACHED */
	}
	return error;
}

/*
 * asvo_klock - kernel version of locking - faster.
 * used by useracc and friends
 * The flags indicate whether AS_LOCK/AS_UNLOCK and direction
 * (AS_READ/AS_WRITE)
 */
/* ARGSUSED */
int
pas_klock(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	uvaddr_t vaddr,		/* start address */
	size_t len,		/* # of bytes */
	int flags,		/* flags/direction */
	int *cookie)		/* cookie */
{
	panic("as_klock");
	return ENOSYS;
}

/*
 * asvo_addspace - generic routine to add mappings
 * different things happen based on the mapping type. Some are
 * pretty specific..
 * Subsumes (parts of) specmap(),execmap(), fs_mapsubr()
 */
int
pas_addspace(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	as_addspace_t *arg,	/* parameters */
	as_addspaceres_t *asres)/* returned info */
{
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;

	switch (arg->as_op) {
	case AS_ADD_MMAPDEV:
		return pas_addmmapdevice(pas, ppas, arg, &asres->as_addr);
	case AS_ADD_MMAP:
		return pas_addmmap(pas, ppas, arg, &asres->as_addr);
	case AS_ADD_EXEC:
		ASSERT(mrislocked_update(&pas->pas_aspacelock));
		return pas_addexec(pas, ppas, arg, &asres->as_addr);
	case AS_ADD_STACKEXEC:
		ASSERT(mrislocked_update(&pas->pas_aspacelock));
		return pas_addstackexec(pas, ppas, arg, &asres->as_addr);
	case AS_ADD_LOAD:
		ASSERT(mrislocked_update(&pas->pas_aspacelock));
		return pas_addload(pas, ppas, arg, &asres->as_addr);
	case AS_ADD_PRDA:
		return pas_addprda(pas, ppas, arg, &asres->as_addr);
	case AS_ADD_ATTACH:
		return pas_addattach(pas, ppas, arg, asres);
	case AS_ADD_SHM:
		return pas_addshm(pas, ppas, arg, &asres->as_addr);
	case AS_ADD_INIT:
		return pas_addinit(pas, ppas, arg, &asres->as_addr);
	default:
		panic("as_addspace");
	}
	/* NOTREACHED */
}

/*
 * asvo_deletespace - remove parts of address space
 * (munmap, exec)
 */
int
pas_deletespace(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	as_deletespace_t *arg,	/* parameters */
	as_deletespaceres_t *res)
{
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;
	int error = 0;

	switch (arg->as_op) {
	case AS_DEL_EXIT:
	case AS_DEL_VRELVM:
		error = pas_delexit(pas, ppas, arg);
		break;
	case AS_DEL_EXEC:
		error = pas_delexec(pas, ppas, arg);
		break;
	case AS_DEL_MUNMAP:
		if (! (arg->as_munmap_flags & DEL_MUNMAP_NOLOCK))
			mrupdate(&pas->pas_aspacelock);
		error = pas_unmap(pas, ppas, arg->as_munmap_start,
				arg->as_munmap_len, arg->as_munmap_flags);
		if (! (arg->as_munmap_flags & DEL_MUNMAP_NOLOCK))
			mrunlock(&pas->pas_aspacelock);
		break;
	case AS_DEL_SHM:
		mrupdate(&pas->pas_aspacelock);
		error = pas_delshm(pas, ppas, arg->as_shm_start, res);
		mrunlock(&pas->pas_aspacelock);
		break;

	case AS_DEL_EXEC_SMEM:
		mrupdate(&pas->pas_aspacelock);
		if (pas->pas_smem_reserved)
			unreservemem(GLOBAL_POOL, pas->pas_smem_reserved, 0, 0);
		pas->pas_smem_reserved = 0;
		mrunlock(&pas->pas_aspacelock);
		break;
	default:
		panic("as_deletespace");
	}

	return error;
}

int
pas_grow(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	as_growop_t op,		/* type */
	uvaddr_t uvaddr)	/* new address to grow to */
{
	int error;
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;

	switch (op) {
	case AS_GROWBRK:
		error = pas_break(pas, ppas, uvaddr);
		break;
	case AS_GROWSTACK:
		error = pas_stackgrow(pas, ppas, uvaddr);
		break;
	default:
		panic("as_grow");
	}
	return error;
}

/*
 * asvo_procio - read/write bytes from one address space into
 * a buffer
 * For non-valid pages this is done via a surrogate rather than
 * attaching into the callers space. This surrogate can run on
 * the local node.
 */
/* ARGSUSED */
int
pas_procio(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	uio_t *uio,	/* target space to read/write */
	void *buf,	/* result/data buffer */
	size_t len)	/* length of buffer */
{
	panic("as_procio");
	return ENOSYS;
}

/*
 * asvo_new - do fork/sproc/uthread processing
 */
int
pas_new(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* caller's private address space id */
	as_new_t *asn,
	as_newres_t *asres)
{
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;
	int error;

	switch (asn->as_op) {
	case AS_FORK:
	case AS_NOSHARE_SPROC:
		error = pas_fork(pas, ppas, asres,
			asn->as_stkptr, asn->as_stklen, asn->as_utas,
			asn->as_op);
		break;
	case AS_SPROC:
		error = pas_sproc(pas, ppas, asres,
			asn->as_stkptr, asn->as_stklen, asn->as_utas);
		break;
	case AS_UTHREAD:
		error = pas_uthread(pas, ppas, asres, asn->as_utas);
		break;
	default:
		panic("pas_new");
	}

	return error;
}

/*
 * asvo_validate - validate an address change. optionally perform
 * any auto-grow that needs to happen
 */
/* ARGSUSED */
int
pas_validate(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	uvaddr_t vaddr,	/* address */
	size_t len,	/* length */
	uint_t flag)	/* flag */
{
	panic("as_validate");
	return ENOSYS;
}

/*
 * pas_verifyvnmap - determine if a vnode is mapped within a specified
 *		  range of our virtual address space.  returns a set
 *		  of vnmap_t's describing the range of virtual addresses
 *		  within the range of virtual addresses touched by the
 *		  biomove associated with the i/o that
 *
 *		  1)  overlap and therefore risk triggering a pagewait
 *		  	deadlock out of the biomove or
 *		  2)  are autogrow and therefore may require that
 *			read operations take write-locks to allow the file
 *			to be grown by the biomoves issued by the read or
 *		  3)  are mapped to the same file and therefore require
 *			that recursive locking be enabled to avoid possible
 *			deadlocks when page-faulting if an updater is waiting
 *			on the file lock
 *
 *		  upon return, for each region that is mapped to the file
 *		  and overlaps the range of addresses to be biomoved,
 *		  vnmap_vaddr/vnmap_len are set to the start/length of
 *		  the overlapping range of virtual addresses.
 *
 *		  vnmap_flags - AS_VNMAP_FOUND is set if the region is
 *				mapped to the file.
 *			      - AS_VNMAP_OVERLAP is set if the region
 *				has a file offset overlap.
 *			      - AS_VNMAP_AUTOGROW is set if the region
 *				might autogrow because of the biomove
 *				
 *		  vnmap_ovvaddr/vnmap_ovlen are set to the start/length
 *		  of the range of virtual addresses within the range of
 *		  addresses being biomoved whose corresponding file offsets
 *		  overlap the offsets directly read/written by the i/o.
 *		  if the region has no overlaps then vnmap_ovvaddr/vnmap_ovlen
 *		  are set to the same values as vnmap_vaddr/vnmap_len.
 *
 *		  vnmap_ovoffset is set to the file offset corresponding
 *		  to vnmap_ovvaddr.
 *
 *		  as_rescodes is set to the cumulative logical OR of all
 *		  vnmap_flags fields.
 *
 *		  The caller specifies the number of maps it can deal with.
 *		  If the number of maps returned is greater than that,
 *		  *all* the maps are allocated and returned in the multimap
 *		  field with mapsize set appropriately.  In case of
 *		  such a map overflow, the multimap must be freed by the
 *		  caller.  Map overflow is signalled by setting multimap
 *		  non-NULL.
 *
 *		  The caller MUST hold the aspacelock before calling this
 *		  routine.
 */
int
pas_verifyvnmap(
	bhv_desc_t		*bhv,
	aspasid_t		pasid,
	as_verifyvnmap_t	*args,
	as_verifyvnmapres_t	*results)
{
	pas_t		*pas;			/* shared address space */
	ppas_t		*ppas;			/* private address space */
	int		nmaps;
	int		maxmaps;
	vnmap_t		*map;
	preg_t		*pregp;
	int		i;
	int		done;
	uint		retval;
	preg_set_t	*preglist;
	uvaddr_t	startvaddr;	/* start/end vaddr of user I/O */
	uvaddr_t	endvaddr;
	uvaddr_t	reg_vaddr_start;/* start/end of virtual addresses */
					/* of mapped region (open range) */
	uvaddr_t	reg_vaddr_end;
	uvaddr_t	iom_vaddr_start; /* ditto for addresses touched */
	uvaddr_t	iom_vaddr_end;	/* by the biomove in the region */
	__psunsigned_t	start_trim;
	__psunsigned_t	end_trim;
	off_t		reg_file_start;	/* first file offset corresponding */
					/* to mapped region */
	off_t		file_start;	/* file i/o offset start/end */
	off_t		file_end;	/* 	(page aligned) */
	off_t		ov_file_start;	/* file offset of overlap area */
	off_t		ov_file_len;
	off_t		iom_file_start;	/* file offsets touched by the */
	off_t		iom_file_end;	/* biomove in a region */
	off_t		iom_file_len;
	off_t		io_start_trim;
	off_t		io_end_trim;

	pas = BHV_TO_PAS(bhv);
	ASSERT(mrislocked_any(&pas->pas_aspacelock));

	results->as_nmaps = 0;
	results->as_multimaps = map = NULL;
	results->as_mapsize = 0;
	results->as_maxoffset = -1;

	if (args->as_len <= 0)
		return 0;

	ppas = (ppas_t *)pasid;

	/*
	 * align maxvaddr up to a page boundary.  as_vaddr has to come in
	 * page-aligned
	 */
	ASSERT(ctob(btoc(args->as_vaddr)) == (__psunsigned_t) args->as_vaddr);

	startvaddr = args->as_vaddr;
	endvaddr = startvaddr + ctob(btoc(args->as_len));
	maxmaps = args->as_nmaps;
	map = args->as_vnmap;

	/*
	 * align I/O start/stop to page boundaries.  Start gets
	 * rounded down, end rounded up.
	 */
	file_start = ctooff(offtoct(args->as_offstart));
	file_end = ctooff(offtoc(args->as_offend));

	do {
		done = 1;
		nmaps = 0;
		retval = 0;

		for (i = 0, preglist = &pas->pas_pregions;
		     i < 2;
		     i++, preglist = &ppas->ppas_pregions) {
			/*
			 * Walk address space, track number of maps.
			 * Store maps.  If we encounter more maps than
			 * the caller supplied us with room, then forget
			 * about storing the map info away.  Just keep counting
			 * so we can allocate the proper number of maps and
			 * do it all over again.
			 */
			pregp = PREG_FINDANYRANGE(preglist,
					  (__psunsigned_t) args->as_vaddr,
					  (__psunsigned_t) args->as_vaddr +
								args->as_len,
						  PREG_EXCLUDE_ZEROLEN);
			if (pregp == NULL)
				continue;
			/*
			 * walk regions and count maps until we're out
			 * of the candidate virtual address range
			 */
			while (pregp != NULL && pregp->p_regva < endvaddr) {
				/*
				 * skip over pregions that don't match
				 */
				if (pregp->p_reg->r_vnode == NULL ||
				    !VN_CMP(args->as_vp,
					    pregp->p_reg->r_vnode)) {
					pregp = PREG_NEXT(pregp);
					continue;
				}

				/*
				 * if we've overrun the set of pre-allocated
				 * maps, signal need for second pass and
				 * just count the regions
				 */
				nmaps++;
				if (nmaps > maxmaps) {
					done = 0;
					pregp = PREG_NEXT(pregp);
					continue;
				}

				/*
				 * start setting up map
				 */
				retval |= AS_VNMAP_FOUND;
				map->vnmap_flags = AS_VNMAP_FOUND;

				/*
				 * calculate the range of virtual addresses
				 * that will be touched by the biomove
				 * for this i/o.  this math is probably
				 * non-optimal but correctness and
				 * comprehensibility is more important.
				 */
				reg_vaddr_start = pregp->p_regva;
				reg_vaddr_end = reg_vaddr_start +
						ctob(pregp->p_pglen);

				start_trim = (startvaddr > reg_vaddr_start) ?
						startvaddr - reg_vaddr_start :
						0;
				end_trim = (reg_vaddr_end > endvaddr) ?
						reg_vaddr_end - endvaddr :
						0;

				iom_vaddr_start = reg_vaddr_start + start_trim;
				iom_vaddr_end = reg_vaddr_end - end_trim;

				ASSERT(ctob(btoc(reg_vaddr_start)) ==
				       (__psunsigned_t) reg_vaddr_start);
				ASSERT(ctob(btoc(reg_vaddr_end)) ==
				       (__psunsigned_t) reg_vaddr_end);

				/*
				 * calculate the range of the file (offsets)
				 * that are mapped by the range of virtual
				 * addresses touched by the biomove for
				 * this i/o
				 */
				reg_file_start = pregp->p_reg->r_fileoff +
						ctooff(pregp->p_offset);
				iom_file_start = reg_file_start + start_trim;

				iom_file_len = ctob(pregp->p_pglen)
						- start_trim - end_trim;
				iom_file_end = iom_file_start + iom_file_len;

				ASSERT(ctob(btoc(iom_file_start)) ==
				       iom_file_start);
				ASSERT(ctob(btoc(iom_file_end)) ==
				       iom_file_end);

				/*
				 * check for an autogrow region -- there can
				 * be only one (swish) ...
				 *
				 * If it is autogrow, determine the max file
				 * offset that will be touched by the biomove
				 * to/from this region.
				 */
				if (pregp->p_reg->r_flags & RG_AUTOGROW) {
					ASSERT((retval & AS_VNMAP_AUTOGROW)
					       == 0);
					retval |= AS_VNMAP_AUTOGROW;
					map->vnmap_flags |= AS_VNMAP_AUTOGROW;
					results->as_maxoffset =
							iom_file_end - 1;
				}

				/*
				 * determine if we have an overlap of
				 * file offsets touched by the i/o and
				 * file offsets touched by the biomove.
				 */
				io_start_trim = (file_start > iom_file_start) ?
						file_start - iom_file_start :
						0;
				io_end_trim = (iom_file_end > file_end) ?
						iom_file_end - file_end :
						0;

				ov_file_start = iom_file_start + io_start_trim;
				ov_file_len = iom_file_len - io_start_trim
						- io_end_trim;

				if (ov_file_len > 0) {
					retval |= AS_VNMAP_OVERLAP;
					map->vnmap_flags |= AS_VNMAP_OVERLAP;
				}

				/*
				 * indicate which user vaddrs (pages)
				 * in this region need to be useracc'ed and/or
				 * biomoved as a group for the indicated
				 * i/o to work and record info in the map.
				 * virtual addresses that map to overlapping
				 * file offset ranges need to be useracced.
				 */
				map->vnmap_vaddr = iom_vaddr_start;
				map->vnmap_len = iom_vaddr_end -
						 iom_vaddr_start;

				if (ov_file_len > 0) {
					map->vnmap_ovvaddr = reg_vaddr_start +
							(ov_file_start -
							 reg_file_start);
					map->vnmap_ovlen = ov_file_len;
					map->vnmap_ovoffset = ov_file_start;
				} else {
					map->vnmap_ovvaddr = iom_vaddr_start;
					map->vnmap_ovlen = map->vnmap_len;
					map->vnmap_ovoffset = iom_file_start;
				}
				ASSERT(map->vnmap_ovvaddr >= map->vnmap_vaddr);
				map++;
				pregp = PREG_NEXT(pregp);
			}
		}

		/*
		 * allocate map structure if we need a second pass and
		 * set return results structure appropriately
		 */
		if (!done) {
			map = kmem_alloc(nmaps * sizeof(vnmap_t), KM_SLEEP);
			if (map == NULL)
				return ENOMEM;
			maxmaps = nmaps;
			results->as_multimaps = map;
			results->as_mapsize = nmaps * sizeof(vnmap_t);
		}
	} while (!done);

	results->as_rescodes = retval;
	results->as_nmaps = nmaps;

	return 0;
}

/*
 * asvo_getsattr - get selected information about a piece of address
 * space.
 *
 * All the masks aren't necessarily permitted in one getattr request -
 * some require the aspacelock, some don't
 */
int
pas_getasattr(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	uint_t amask,		/* mask of what to get */
	as_getasattr_t *attr)	/* what to get & return value */
{
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;

#if DEBUG
	switch (amask) {
	case AS_PSIZE:
	case AS_RSSSIZE:
	case AS_PSIZE|AS_RSSSIZE:
	case AS_RSSMAX:
	case AS_DATAMAX:
	case AS_STKMAX:
	case AS_VMEMMAX:
	case AS_BRKMAX:
	case AS_ISOMASK:
	case AS_PRDA:
	case AS_VPAGG:
		break;
	default:
		ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
			(mrislocked_update(&pas->pas_aspacelock)));
	}
#endif
	if (amask & AS_VMEMMAX) {
		attr->as_vmemmax = pas->pas_vmemmax;
	}
	if (amask & AS_DATAMAX) {
		attr->as_datamax = pas->pas_datamax;
	}
	if (amask & AS_RSSMAX) {
		attr->as_rssmax = pas->pas_rssmax;
	}
	if (amask & AS_PSIZE) {
		/* does NOT include phys regions */
		mutex_lock(&pas->pas_preglock, 0);
		attr->as_psize = pas->pas_size + ppas->ppas_size;
		mutex_unlock(&pas->pas_preglock);
	}
	if (amask & AS_RSSSIZE) {
		mutex_lock(&pas->pas_preglock, 0);
		attr->as_rsssize = pas->pas_rss + ppas->ppas_rss;
		mutex_unlock(&pas->pas_preglock);
	}
	if (amask & AS_STKMAX) {
		if (ppas->ppas_flags & PPAS_STACK)
			attr->as_stkmax = ppas->ppas_stkmax;
		else
			attr->as_stkmax = pas->pas_stkmax;
	}
	if (amask & AS_BRKBASE) {
		attr->as_brkbase = pas->pas_brkbase;
	}
	if (amask & AS_BRKSIZE) {
		attr->as_brksize = pas->pas_brksize;
	}
	if (amask & AS_STKBASE) {
		if (ppas->ppas_flags & PPAS_STACK)
			attr->as_stkbase = ppas->ppas_stkbase;
		else
			attr->as_stkbase = pas->pas_stkbase;
	}
	if (amask & AS_STKSIZE) {
		if (ppas->ppas_flags & PPAS_STACK)
			attr->as_stksize = ppas->ppas_stksize;
		else
			attr->as_stksize = pas->pas_stksize;
	}
	if (amask & AS_BRKMAX) {
		pas_ulimit(pas, ppas, attr);
	}
	if (amask & AS_LOCKDOWN) {
		attr->as_lockdown = 0;
		if (pas->pas_lockdown & AS_PROCLOCK)
			attr->as_lockdown |= PROCLOCK;
		if (pas->pas_lockdown & AS_TEXTLOCK)
			attr->as_lockdown |= TXTLOCK;
		if (pas->pas_lockdown & AS_DATALOCK)
			attr->as_lockdown |= DATLOCK;
		if (pas->pas_lockdown & AS_FUTURELOCK)
			attr->as_lockdown |= FUTURELOCK;
	}
	if (amask & AS_ISOMASK) {
		attr->as_isomask = pas->pas_isomask;
	}
	if (amask & AS_VPAGG) {
		attr->as_vpagg = pas->pas_vpagg;
	}
	if (amask & AS_PRDA) {
		preg_t *prp;

		/*
		 * XXX fork used to check for anon_lookup as well
		 * but that seems superfluous since how can one have
		 * a PRDA region w/ no pages??
		 */
		mraccess(&pas->pas_aspacelock);
		if (prp = findpregtype(pas, ppas, PT_PRDA))
			attr->as_prda = prp->p_regva;
		else
			attr->as_prda = AS_ADD_UNKVADDR;
		mrunlock(&pas->pas_aspacelock);
	}
	return 0;
}

/*
 * asvo_getvaddrattr - get selected information about a virtual addr
 *
 * All the masks aren't necessarily permitted in one getattr request -
 * some require the aspacelock, some don't
 */
int
pas_getvaddrattr(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	uint_t amask,		/* mask of what to get */
	uvaddr_t uvaddr,
	as_getvaddrattr_t *attr)/* what to return value */
{
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;
	preg_t *prp;
	reg_t *rp;
	int error = 0;

#if DEBUG
	switch (amask) {
	case AS_MPROT:
		break;
	default:
		ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
			(mrislocked_update(&pas->pas_aspacelock)));
	}
#endif
	if (amask & (AS_FLAGS|AS_VNODE|AS_LOADVNODE|AS_CKPTMAP|AS_PINFO)) {
		if ((prp = findpreg(pas, ppas, uvaddr)) == NULL)
			return EINVAL;
	}

	if (amask & AS_VNODE)
		attr->as_vp = prp->p_reg->r_vnode;
	if (amask & AS_LOADVNODE) {
		if (prp->p_reg->r_flags & RG_LOADV)
			attr->as_loadvp = prp->p_reg->r_loadvnode;
		else
			attr->as_loadvp = NULL;
	}
#if CKPT
	if (amask & AS_CKPTMAP) {
		attr->as_ckpt.as_ckptflags = prp->p_reg->r_ckptflags;
		attr->as_ckpt.as_ckptinfo = prp->p_reg->r_ckptinfo;
	}
#endif
	if (amask & AS_FLAGS) {
		rp = prp->p_reg;
		attr->as_flags = 0;
		if (rp->r_flags & RG_PHYS)
			attr->as_flags |= AS_PHYS;
	}
	if (amask & AS_PINFO) {
		/* prp set above */
		attr->as_pinfo.as_base = prp->p_regva;
		attr->as_pinfo.as_len = ctob(prp->p_pglen);
	}
	if (amask & AS_MPROT) {
		attr_t *pattr;

		mraccess(&pas->pas_aspacelock);
		if ((prp = findpreg(pas, ppas, uvaddr)) == NULL)
			error = EINVAL;
		else {
			reglock(prp->p_reg);
			pattr = findattr(prp, uvaddr);
			attr->as_mprot = pattr->attr_prot;
			regrele(prp->p_reg);
		}
		mrunlock(&pas->pas_aspacelock);
		if (error)
			return error;
	}
	return error;
}

/*
 * asvo_getattr - get complex info
 */
int
pas_getattr(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	as_getattrop_t op,	/* what to get */
	as_getattrin_t *in,	/* input parms */
	as_getattr_t *attr)	/* what to get & return value */
{
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;
	int error = 0;

	switch (op) {
	case AS_GET_PRATTR:
		error = pas_getprattr(pas, ppas, in, attr);
		break;
	case AS_GET_PAGERINFO:
		{
		/*
		 * scan all ppas's looking for shortest slptime and
		 * highest pri
		 * We also set a 'highest' state which controls how hard
		 * to page someone.
		 * We view RT folks as never sleeping
		 * so they'll get placed at the bottom of the tree.
		 * We view batch folks as always sleeping ..
		 */
		int s;
		uthread_t *ut;
		short pri, state, tstate;

		attr->as_pagerinfo.as_pri = SHRT_MIN;
		attr->as_pagerinfo.as_slptime = INT_MAX;
		state = GTPGS_WEIGHTLESS; /* worst */
		mraccess(&pas->pas_plistlock);
		for (ppas = pas->pas_plist; ppas; ppas = ppas->ppas_next) {
			/* 
			 * if any thread is isolated - forget the rest
			 */
			if (ppas->ppas_flags & PPAS_ISO) {
				state = GTPGS_ISO;
				break;
			}
			s = mutex_spinlock(&ppas->ppas_utaslock);
			if (ppas->ppas_utas) {
				ut = ppas->ppas_utas->utas_ut;
				if (KT_ISPS(UT_TO_KT(ut))) {
					if (!KT_ISNPRMPT(UT_TO_KT(ut)))
						tstate = GTPGS_PRI_HI;
					else
						tstate = GTPGS_PRI_NORM;
				} else if (is_weightless(UT_TO_KT(ut)))
					tstate = GTPGS_WEIGHTLESS;
				else
					tstate = GTPGS_NORM;
				state = MAX(state, tstate);

				/* set slptime */
				switch (state) {
				case GTPGS_NORM:
					if (attr->as_pagerinfo.as_slptime != 0) {
						long slptime = ktimer_slptime(UT_TO_KT(ut));
						if (slptime < attr->as_pagerinfo.as_slptime)
							attr->as_pagerinfo.as_slptime = slptime;
					}
					break;
				case GTPGS_PRI_HI:
				case GTPGS_PRI_NORM:
					attr->as_pagerinfo.as_slptime = 0;
					break;
				}
				pri = kt_basepri(UT_TO_KT(ut));
				if (pri > attr->as_pagerinfo.as_pri)
					attr->as_pagerinfo.as_pri = pri;
			} else { /* No uthread. Can be skipped */
				state = GTPGS_DEFUNCT;
			}
			
			mutex_spinunlock(&ppas->ppas_utaslock, s);
		}
		attr->as_pagerinfo.as_state = state;
		mrunlock(&pas->pas_plistlock);
		}
		break;

	case AS_GFXLOOKUP:
	case AS_INUSE:
	case AS_CKPTGETMAP:
		mraccess(&pas->pas_aspacelock);
		error = pas_getattrscan(pas, ppas, op, in, attr);
		mrunlock(&pas->pas_aspacelock);
		break;

	case AS_NATTRS:
	case AS_GFXRECYCLE:
		ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
			(mrislocked_update(&pas->pas_aspacelock)));
		error = pas_getattrscan(pas, ppas, op, in, attr);
		break;
	case AS_GET_KVMAP:
		ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
			(mrislocked_update(&pas->pas_aspacelock)));
		error = pas_getkvmap(pas, ppas, attr);
		break;
	default:
		panic("pas_getattr");
		/* NOTREACHED */
	}

	return error;
}

/*
 * Set attributes. Note that some operations require the aspacelock held
 * others do not
 */
/* ARGSUSED */
int
pas_setattr(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	uvaddr_t vaddr,		/* address */
	as_setattr_t *attr)	/* what to set & info about setting */
{
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;
	int ilocked = 0;
	preg_t *prp;
	reg_t *rp;
	int doingshd = 0;
	int error = 0;

	switch (attr->as_op) {
	case AS_SET_PMAP:
		break;
	default:
		mraccess(&pas->pas_aspacelock);
		ilocked = 1;
		break;
	}

	switch (attr->as_op) {
	case AS_SET_VMEMMAX:
		pas->pas_vmemmax = attr->as_set_max;
		break;
	case AS_SET_DATAMAX:
		pas->pas_datamax = attr->as_set_max;
		break;
	case AS_SET_STKBASE:
		if (ppas->ppas_flags & PPAS_STACK)
			ppas->ppas_stkbase = attr->as_set_stkbase;
		else
			pas->pas_stkbase = attr->as_set_stkbase;
		break;
	case AS_SET_STKSIZE:
		if (ppas->ppas_flags & PPAS_STACK)
			ppas->ppas_stksize = attr->as_set_stksize;
		else
			pas->pas_stksize = attr->as_set_stksize;
		break;
	case AS_SET_STKMAX:
		if (ppas->ppas_flags & PPAS_STACK)
			ppas->ppas_stkmax = attr->as_set_max;
		else
			pas->pas_stkmax = attr->as_set_max;
		break;
	case AS_SET_RSSMAX:
		/* wrapping, ie msb being set in btoc64 is okay */
		pas->pas_rssmax = attr->as_set_max;
		if (btoc64(pas->pas_rssmax) > INT_MAX)
			pas->pas_maxrss = INT_MAX;
		else
			pas->pas_maxrss = btoc64(pas->pas_rssmax);
		break;
	case AS_SET_ISOLATE:
		ppas_flagset(ppas, PPAS_ISO);
		prp = PREG_FIRST(ppas->ppas_pregions);

doshd:
		for ( ; prp ; prp = PREG_NEXT(prp)) {
			rp = prp->p_reg;
			reglock(rp);
			rp->r_flags |= RG_ISOLATE;
			regrele(rp);
		}
		if (!doingshd) {
			prp = PREG_FIRST(pas->pas_pregions);
			doingshd = 1;
			goto doshd;
		}
		CPUMASK_SETB(pas->pas_isomask, attr->as_iso_cpu);
		break;
	case AS_SET_UNISOLATE:
		{ int unisoshd = 1;
		prp = PREG_FIRST(ppas->ppas_pregions);

		/*
		 * if (cpu == PDA_RUNANYWHERE)
		 *	The appropriate thing to do would be to clear and
		 *	recreate the isolated cpu bitmask.  However, currently,
		 *	there isn't really a reliable way to do this.
		 *	A thread may have been moved off of its isolated
		 *	cpu by a system call or driver.
		 *	It would be possible to have restoremustrun() check
		 *	if it's moving a process back to an isolated cpu and if
		 *	so, update the cpumask + check for missed tlb or icache
		 *	flushes.  For now, simply leave the bitmask alone.  If
		 *	this is the last process on the isolated cpu, the
		 *	bitmask will have a false 1 here until either another
		 *	process in the share group mustruns itself to the cpu
		 *	or the share group terminates.
		 */
		if (attr->as_iso_cpu != PDA_RUNANYWHERE) {
			/* remove this cpu from the bitmask */
			CPUMASK_CLRB(pas->pas_isomask, attr->as_iso_cpu);
		}
		if (!CPUMASK_IS_ZERO(pas->pas_isomask))
			/* Still an thread on isolated cpus */
			unisoshd = 0;

dounisoshd:
		for ( ; prp ; prp = PREG_NEXT(prp)) {
			rp = prp->p_reg;
			reglock(rp);
			/*
			 * We may be sharing this region. Only unisolate it if
			 * has only 1 user
			 */
			if (rp->r_refcnt == 1)
				rp->r_flags &= ~RG_ISOLATE;
			regrele(rp);
		}
		if (!doingshd && unisoshd) {
			doingshd = 1;
			prp = PREG_FIRST(pas->pas_pregions);
			goto dounisoshd;
		}
		ppas_flagclr(ppas, PPAS_ISO);
		}
		break;
#if ULI
	case AS_SET_ULI:
		if (attr->as_set_uli) {
			/*
			 * set us as a ULI this means:
			 * 1) no paging
			 * 2) always in segtbl mode
			 * 3) XXX don't we have to really restrict
			 * anything that needs to zapsegtbl
			 *
			 * For easy testing we set ULI in the utas as
			 * well. We are always called on the 'current' utas
			 * so no locking necessary.
			 */
			pas_inculi(pas);
			utas_flagset(ppas->ppas_utas, UTAS_ULI);
			initsegtbl(ppas->ppas_utas, pas, ppas);
		} else {
			/* unset */
			ASSERT(pas->pas_flags & PAS_ULI);
			pas_deculi(pas);
			ASSERT(ppas->ppas_utas->utas_flag & UTAS_ULI);
			utas_flagclr(ppas->ppas_utas, UTAS_ULI);
		}
		break;
#endif
#if CKPT
#if _MIPS3_ADDRSPACE
	case AS_SET_PMAP:
		error = replace_pmap(pas, ppas, attr->as_pmap_abi);
		break;
#endif
	case AS_SET_CKPTBRK:
		mutex_lock(&pas->pas_brklock, 0);
		pas->pas_brkbase = attr->as_set_brkbase;
		pas->pas_brksize = attr->as_set_brksize;
		mutex_unlock(&pas->pas_brklock);
		break;
#endif
	case AS_SET_VPAGG:
		pas->pas_vpagg = attr->as_set_vpagg;
		/*
		 * Don't do Miser vm acct for this process. Miser Vm accounting
		 * is done for this process's children and on execs. 
		 */
		pas_flagset(pas, PAS_NOMEM_ACCT);
                break;
	default:
		panic("as_setattr");
	}

	if (ilocked)
		mrunlock(&pas->pas_aspacelock);

	return error;
}

/*
 * asvo_getrangeattr - get attributes covering a range of address space
 * Covers:
 *	virtual to phys mappings (vtopv)
 */
/* ARGSUSED */
int
pas_getrangeattr(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	uvaddr_t vaddr,		/* address */
	size_t len,		/* length */
	as_getrangeattrop_t op,
	as_getrangeattrin_t *in,/* info about what to get */
	as_getrangeattr_t *attr)/* returned info */
{
	int error = 0;
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;

	switch (op) {
	case AS_GET_ADDR:
		{
		uvaddr_t vaddr;
		vaddr = pas_allocaddr(pas, ppas, btoc(len), in->as_color);
		if (vaddr == NULL)
			error = ENOMEM;
		else
			attr->as_addr = vaddr;
		}
		break;
	case AS_GET_PGD:
		error = pas_getpginfo(pas, ppas, vaddr, len, in, attr);
		break;
	case AS_GET_VTOP:
	case AS_GET_PDE:
	case AS_GET_KPTE:
	case AS_GET_ALEN:
		error = pas_vtopv(pas, ppas, op, vaddr, len, in, attr);
		break;
	default:
		panic("as_getrangeattr");
	}
	return error;
}

/*
 * asvo_setrangeattr - set attributes covering a range of address space
 * Covers:
 *	locking (mpin)
 *	protections (mprotect)
 *	caching
 *	sync/flush (madvise/msync)
 */
/* ARGSUSED */
int
pas_setrangeattr(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	uvaddr_t vaddr,		/* address */
	size_t len,		/* length */
	as_setrangeattr_t *attr,/* info about what to set */
	as_setrangeattrres_t *asres) /* returned info */
{
	int error = 0;
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;

	switch (attr->as_op) {
	case AS_UNLOCK_BY_ATTR:
	case AS_UNLOCK:
	case AS_UNLOCKALL:
		mrupdate(&pas->pas_aspacelock);
		error = pas_memlock(pas, ppas, vaddr, len, attr);
		break;
	case AS_LOCK_BY_ATTR:
	case AS_LOCK:
	case AS_LOCKALL:
		mraccess(&pas->pas_aspacelock);
		error = pas_memlock(pas, ppas, vaddr, len, attr);
		break;
	case AS_PROT:
		/* let mprotect do locking */
		error = pas_mprotect(pas, ppas, vaddr, len,
				attr->as_prot_prot, attr->as_prot_flags);
		return error;
		/* NOTREACHED */
	case AS_ADVISE:
		/* let madvise do locking */
		error = pas_madvise(pas, ppas, vaddr, len, attr->as_advise_bhv);
		return error;
		/* NOTREACHED */
	case AS_SYNC:
		mraccess(&pas->pas_aspacelock);
		error = pas_msync(pas, ppas, vaddr, len, attr->as_sync_flags);
		break;
	case AS_CACHEFLUSH:
		{
		pde_t *pd;
		reg_t *rp;
		preg_t *prp;

		mraccess(&pas->pas_aspacelock);
		if ((prp = findpreg(pas, ppas, vaddr)) == NULL) {
			error = ENOENT;
			break;
		}
		rp = prp->p_reg;
		reglock(rp);
		pd = pmap_pte(pas, prp->p_pmap, vaddr, VM_NOSLEEP);
		if (pd == NULL || pg_isnoncache(pd)) {
			regrele(rp);
			error = EDOM;
			break;
		}

		if (!pg_isvalid(pd)) {
			/*
			 * We were ignoring this case - this seems
			 * wrong - a page could be invalidated
			 * cause vhand is stealing it - but then the
			 * process could reclaim it - the page
			 * wouldn't necessarily get flushed in this
			 * case. This seems rare so we can just
			 * do a fulll flush.
			 */
			regrele(rp);
			error = EXDEV;
			break;
		}
#ifdef _VCE_AVOIDANCE
		if (vce_avoidance) {
			pfn_t pfn = pg_getpfn(pd);
			int  color = pfn_to_vcolor(pfn);
			caddr_t mapped_address;
			extern uint cachecolorsize;

			if (attr->as_cache_flags & CACH_OTHER_COLORS) {
				if (cachecolorsize > 2) {
					regrele(rp);
					error = EXDEV;
					break;
				}

				mapped_address = (caddr_t)
					page_mapin_pfn(pfntopfdat(pfn),
					(VM_NOSLEEP|VM_OTHER_VACOLOR),
					(colorof(vaddr) ^ 1), pfn);
			} else

#ifdef R4600SC
			if (small_K0_pfn(pfn) &&
				(vcache_color(pfn) == color))

				mapped_address =  
					(caddr_t)vfntova(small_pfntovfn_K0(pfn));
			else
#endif /* R4600SC */
				mapped_address = 
					(caddr_t)page_mapin_pfn(pfntopfdat(pfn),
						       VM_NOSLEEP,-1,
						       pfn);
			if (mapped_address == NULL) {
				regrele(rp);
				error = EXDEV;
				break;
			}

			_bclean_caches(mapped_address + poff(vaddr), len,
				pfn, attr->as_cache_flags|CACH_VADDR);
			page_mapout(mapped_address);
		} else
#endif	/* _VCE_AVOIDANCE */
		{
			pfd_t* pfd = pdetopfdat_hold(pd);
			
			if (!pfd) {
				regrele(rp);
				error = EXDEV;
				break;
			}

			_bclean_caches(vaddr, len, pfdattopfn(pfd),
					attr->as_cache_flags|CACH_VADDR);
			pfdat_release(pfd);
		}
		regrele(rp);
		break;
		}
	case AS_CACHECTL:
		{
		pgcnt_t bpcnt;
		uint op = attr->as_cachectl_flags;
		int was_uncached;
		attr_t *attr;
		pde_t attrpde;

		/*
		 * first validate address and grow if necessary
		 * NOTE - upon success leaves aspacelock locked for access
		 */
		if (error = fault_in(pas, ppas, vaddr, len, B_WRITE)) {
			return error;
		}

		attrpde.pgi = 0;
		if (op == UNCACHEABLE)
			pg_setnoncache(&attrpde);
		else
			pg_setcachable(&attrpde);

		bpcnt = btoc(len);
		do {
			register preg_t	*prp;
			register reg_t	*rp;
			register pde_t	*pt;
			uvaddr_t	end;
			auto pgno_t	sz;

			prp = findpreg(pas, ppas, vaddr);
			if (prp == NULL) {
				/* address not in process space */
				error = EFAULT;
				break;
			}
			rp = prp->p_reg;
			reglock(rp);

			sz = MIN(bpcnt, pregpgs(prp, vaddr));

			/*
			 * SPT
			 * Privatize shared PT's if necessary 
			 * (see comment in pmap.c)
			 */
			prp->p_nvalid += pmap_modify(pas, ppas, 
							prp, vaddr, sz);
		
			pt = pmap_ptes(pas, prp->p_pmap, vaddr, &sz, 0);
			ASSERT(sz >= 0 && sz <= bpcnt);
			bpcnt -= sz;
			end = vaddr + ctob(sz);

			attr = findattr_range(prp, vaddr, end);

			pmap_downgrade_addr_boundary(pas, prp, vaddr,
							sz, PMAP_TLB_FLUSH);
			for ( ; ; ) {
				ASSERT(attr);
				attr->attr_cc = attrpde.pte.pg_cc;

				if (attr->attr_end == end)
					break;

				attr = attr->attr_next;
			}

			for ( ; --sz >= 0 ; pt++, vaddr += NBPC) {

				/* XXX	Use attr struct to determine
				 * cache state!
				 */
				was_uncached = pg_isnoncache(pt);

				if (op == UNCACHEABLE)
					pg_setnoncache(pt);
				else
					pg_setcachable(pt);

				/*
				 * If pt not currently associated with phys mem,
				 * don't worry about flushing caches or tlb.
				 *
				 * XXX	If the page has been stolen, and is
				 * XXX	later reclaimed, the i- and d-caches
				 * XXX	will have the old data.  Fix someday,eh?
				 */
				if (!pg_isvalid(pt))
					continue;

				tlbclean(pas, btoct(vaddr), pas->pas_isomask);

				if (op == UNCACHEABLE && (was_uncached == 0)) {
					int pfn = pg_getpfn(pt);
					if (pfn) {
						_bclean_caches(vaddr, NBPC,
							pfn,
							(CACH_FORCE|CACH_ICACHE|CACH_DCACHE|CACH_INVAL|
							((
#ifdef MH_R10000_SPECULATION_WAR
                                                  IS_R10000()
                                                   ? CACH_NOADDR
                                                   :
#endif /* MH_R10000_SPECULATION_WAR */
						CACH_VADDR))));
					}
				}
			}

			regrele(rp);

		} while (bpcnt);
		break;
		}
	case AS_MAKE_PRIVATE:
		mrupdate(&pas->pas_aspacelock);
		if (!ispcwriteable(pas, ppas, vaddr, 0)) {
			error = vgetprivatespace(pas, ppas, vaddr, NULL);
		}
		break;
	default:
		panic("as_setrangeattr");
		/* NOTREACHED */
	}
	mrunlock(&pas->pas_aspacelock);
	return error;
}

/*
 * asvo_lookuppasid - validate a pasid. It is assumed that the caller
 * has a reference to the vas. Upon successful return a reference is held
 * on the pasid
 * Note that the decrement in pas_relepasid holds no locks - so we
 * must atomically check for zero reference
 */
int
pas_lookuppasid(
	bhv_desc_t *bhv,
	aspasid_t pasid)	/* private address space id */
{
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *tp;

	mraccess(&pas->pas_plistlock);
	for (tp = pas->pas_plist; tp; tp = tp->ppas_next) {
		if (tp == (ppas_t *)pasid) {
			/* found it */
			if (compare_and_inc_int_gt_zero(&tp->ppas_refcnt) == 0) {
				/* whoa! ref was zero - this means
				 * we raced with pas_relepasid which is
				 * now waiting in ppas_free for the plistlock
				 */
				break;
			}
			/* found it and valid */
			mrunlock(&pas->pas_plistlock);
			return 0;
		}
	}
	mrunlock(&pas->pas_plistlock);
	return ESRCH;
}

/*
 * asvo_relepasid - release a reference on a pasid
 */
void
pas_relepasid(
	bhv_desc_t *bhv,
	aspasid_t pasid)	/* private address space id */
{
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;

	ASSERT(ppas->ppas_refcnt > 0);
	if (atomicAddInt(&ppas->ppas_refcnt, -1) == 0)
		ppas_free(pas, ppas);
}

/*
 * asvo_destroy - destroy the aso
 */
/* ARGSUSED */
void
pas_destroy(
	bhv_desc_t *bhv,
	aspasid_t pasid)	/* private address space id */
{
	pas_t *pas = BHV_TO_PAS(bhv);

	pas_free(pas);
}

/*
 * asvo_shake - shake various parts of AS
 *
 * NOTE: only SHAKETSAVE handles an instance of a ppas - the rest all shake
 * the entire AS.
 * NOTE2: 
 */
int
pas_shake(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	as_shakeop_t op,
	as_shake_t *shake)
{
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;
	preg_t *prp;
	reg_t *rp;
	int error = 0;


	if (op == AS_SHAKERSS)
		return pas_shakerss(pas);
	else if (op == AS_SHAKEAGE) {
		pas_ageregion(pas);
		return 0;
	} else if (op == AS_SHAKEPAGE) {
		return pas_spinas(pas, shake);
	} else if (op == AS_SHAKETSAVE) {
		/*
		 * Alas, we really kind of need to flush out any tlb entries -
		 * its too bad - most likely the user hasn't touched any
		 * of this stuff, but they could of..
		 */
		if (!(shake->as_shaketsave_flags & AS_TSAVE_LOCKED))
			mrupdate(&pas->pas_aspacelock);
		remove_tsave(pas, ppas,
		 shake->as_shaketsave_flags & AS_NOFLUSH ? RF_NOFLUSH : 0);
		if (!(shake->as_shaketsave_flags & AS_TSAVE_LOCKED))
			mrunlock(&pas->pas_aspacelock);
		return 0;
	}

	if (!mrtryaccess(&pas->pas_aspacelock))
		return EBUSY;

	/* ignore isolated processes */
	if (CPUMASK_IS_NONZERO(pas->pas_isomask)) {
		mrunlock(&pas->pas_aspacelock);
		return EBUSY;
	}

	/* scan each region looking for anon/swap pages */
	mraccess(&pas->pas_plistlock);
	ppas = pas->pas_plist;
        prp = PREG_FIRST(pas->pas_pregions);
dopr:
        for ( ; prp; prp = PREG_NEXT(prp)) {
                rp = prp->p_reg;

		if (op == AS_SHAKEANON) {
			if ((rp->r_flags & RG_PHYS) || !(rp->r_flags & RG_ANON))
				continue;

			if (creglock(rp) == 0)
				continue;

			anon_shake_tree(rp->r_anon);
			regrele(rp);
		} else if (op == AS_SHAKESWAP) {
			if (rp->r_flags & RG_PHYS)
				continue;
			reglock(rp);
			if (error = unswapreg(pas, rp, shake->as_shakeswap_hard,
					shake->as_shakeswap_lswap, prp)) {
				/* unswapreg has released reglock */
				goto out;
			}
			regrele(rp);
		}
	}

        if (ppas) {
                prp = PREG_FIRST(ppas->ppas_pregions);
		ppas = ppas->ppas_next;
                goto dopr;
        }
out:
	mrunlock(&pas->pas_plistlock);
	mrunlock(&pas->pas_aspacelock);
	return error;
}

int
pas_share(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	as_shareop_t op,
	as_share_t *as,		/* input parameters */
	as_shareres_t *asres)	/* return info */
{
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;
	int error;

	switch (op) {
	case AS_PFLIP:
		error = pas_pflip(pas, ppas,
				as->as_share_pflip.as_kvaddr,
				as->as_share_pflip.as_tgtaddr,
				as->as_share_pflip.as_dosync,
				&asres->as_kvaddr);
		break;
	case AS_PATTACH:
		error = pas_pattach(pas, ppas,
				as->as_share_pattach.as_vaddr,
				as->as_share_pattach.as_dosync,
				&asres->as_kvaddr);
		break;
	case AS_PSYNC:
		error = pas_psync(pas, ppas);
		break;
	default:
		panic("pas_share");
		/* NOTREACHED */
	}
	return error;
}


asvo_ops_t pas_ops = {
	BHV_IDENTITY_INIT_POSITION(VAS_POSITION_PAS),
	pas_lock,
	pas_trylock,
	pas_islocked,
	pas_unlock,
	pas_fault,
	pas_klock,
	pas_addspace,
	pas_deletespace,
	pas_grow,
	pas_procio,
	pas_new,
	pas_validate,
	pas_getasattr,
	pas_getvaddrattr,
	pas_getattr,
	pas_setattr,
	pas_getrangeattr,
	pas_setrangeattr,
	pas_lookuppasid,
	pas_relepasid,
	pas_destroy,
	pas_shake,
	pas_watch,
	pas_share,
	pas_verifyvnmap,
};
