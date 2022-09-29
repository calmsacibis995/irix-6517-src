/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.176 $"

#include <string.h>
#include <sys/types.h>
#include <sys/anon.h>
#include <ksys/as.h>
#include <sys/atomic_ops.h>
#include <sys/buf.h>
#include <sys/capability.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/flock.h>
#include <sys/getpages.h>
#include <sys/immu.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/numa.h>
#include <sys/param.h>
#include <sys/page.h>
#include <sys/pfdat.h>
#include <os/as/pmap.h>
#include <sys/prctl.h>
#include <os/as/region.h>
#include <sys/resource.h>
#include <sys/sbd.h>
#include <sys/sema.h>
#include <sys/swap.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <ksys/vproc.h>
#include <ksys/vshm.h>
#include <ksys/vm_pool.h>
#include <ksys/vmmacros.h>
#include <sys/nodepda.h>

#define	EMISER -2

/*
 * XXX - a version of sm_free that already has lock??
 */
struct delarg {
	int hard;
	int lswap;
};

static pgno_t swapfind(register swapinfo_t *st, register int size, int *fsize);
static int shake_swap(int);
static int swapseg(dev_t, struct buf *, int);
extern int swaptabsnap(swapinfo_t **si_snapp);
static int doswapdel(swapinfo_t *st);
static int unswap(pas_t *, pgno_t, preg_t *, int);
static int xswap(pgno_t, preg_t *, reg_t *, int);
static pgno_t _nfreeswap(void);
static void unswapsysreg(void);
static void unswapshm(struct delarg *);
static void do_unswapreg(vshm_t *, void *);

static lock_t swap_spinlock;		/* mutex for all swap entries
					 * protects add/del from alloc/free
					 */
static sema_t inswapmod;		/* single thread add/deletes
					 * protects multiple users from
					 * adding/deleting at same time
					 */
static mutex_t shake_swap_lock;		/* serialize swap shake operations */
static swaptab_t swaptab[NSWAPPRI];	/* access via priority */
swapinfo_t *lswaptab[MAXLSWAP];		/* direct access via logical swap */
static int nswapfiles;			/* current number configured swaps */

#define swaplock()	mp_mutex_spinlock(&swap_spinlock);
#define swapunlock(s)	mp_mutex_spinunlock(&swap_spinlock, s);
#define swapmodlock()	psema(&inswapmod, PZERO);
#define cpswapmodlock()	cpsema(&inswapmod)
#define swapmodunlock()	vsema(&inswapmod);

/*
 * an internal swap handle
 */
typedef union {
	struct {
		__uint32_t	sid_lswap:8,	/* logical swap device */
				sid_pageno:24;	/* page number on device */
	} sid;
	sm_swaphandle_t		sm_all;
} swaphandle_t;
#define sm_lswap sid.sid_lswap
#define sm_pageno sid.sid_pageno

#ifdef DEBUG
int swapwrinject = 0;	/* inject swap write errors */
int swapwrinjectfreq  = 10;
int swaprdinject = 0;	/* inject swap read errors */
int swaprdinjectfreq  = 10;
#endif

void
swapinit(void)
{

	initnsema(&inswapmod, 1, "swapmod");
	spinlock_init(&swap_spinlock, "swap lock");
	mutex_init(&shake_swap_lock, MUTEX_DEFAULT, "swapshk");

	shake_register(SHAKEMGR_SWAP, shake_swap);
}

#undef SWAPDEBUG
#ifdef SWAPDEBUG
static int doswapcksum = 1;

static long
swapcksum(pfd_t *pfd, int size, int rw)
{
	register int csum = 0;
	register int max = size / sizeof(long);
	register int i;
	register long *ptr, *sptr;

	sptr = ptr = (long *)page_mapin(pfd, 0, 0);
		cache_operation(sptr, size,
			CACH_DCACHE|CACH_WBACK|CACH_INVAL|CACH_LOCAL_ONLY|
			CACH_AVOID_VCES|
			((rw & B_READ) : CACH_IO_COHERENCY ? 0) );

	for (i = 0; i < max; i++) {
		csum += *ptr;
		ptr++;
	}
	/* noone could have written to it yet right?? */
	cache_operation(sptr, size,
		CACH_DCACHE|CACH_WBACK|CACH_INVAL|CACH_LOCAL_ONLY|
		CACH_AVOID_VCES);
	page_mapout((caddr_t)sptr);
	return(-(csum+1));
}

static void
swapsum(pglst_t *pglist, swaphandle_t sw, pgno_t size, int rw)
{
	register long *sumptr, psum;
	register swapinfo_t *st;
	pgno_t swappg;
	int i;

	if (!doswapcksum)
		return;
	st = lswaptab[sw.sm_lswap];
	swappg = sw.sm_pageno - st->st_swppglo;
	sumptr = &st->st_cksum[swappg];

	for (i = 0; i < size; i++, sumptr++, pglist++) {
		psum = swapcksum(pglist->gp_pfd, NBPP, rw);
		if (!(rw & B_READ)) {
			*sumptr = psum;
			continue;
		}
		if (*sumptr != psum) {
			cmn_err(CE_WARN,
				"wrong swapsum, pglist 0x%x was %d not %d",
				pglist, psum, *sumptr);
			debug(0);
		}
	}
}
#endif

/*
 * swapctl(2)
 */
struct swapcmda {
	sysarg_t sc_cmd;        /* command code for swapctl     */
	void    *sc_arg;        /* argument pointer for swaptcl */
};

/*
 * int
 * swapctl(struct swapcmda *uap, rval_t *rvp)
 *	System call to add, delete, list, and total swap devices.
 *
 *	On success, zero is returned and the requested operation has
 *	been performed. If (sc_cmd == SC_GETNSWAP), then the number
 *	of swap devices will be returned in rvp->r_val1. 
 *	If (sc_cmd == SC_LIST), then the requested information will have 
 *	been copied out into the user's address space at the address
 *	indicated by ((swaptbl_t *)uap->sc_arg)->swt_ent and rvp->r_val1 
 *	will be set to indicate how many entries were actually returned.
 *
 * 	On failure, a non-zero errno is returned to indicate the failure
 *	mode.
 *
 * Remarks:
 *	When performing SC_ADD and SC_REMOVE requests, offset and length
 *	values passed in sr_start and sr_length are provided in terms
 *	of UBSIZE (512-byte) units and are converted before being used.
 *	See comment in SC_ADD/SC_REMOVE cases below.
 */
int
swapctl(struct swapcmda *uap, rval_t *rvp)
{
	int error = 0;
	xswapres_t xsr;
	struct vnode *vp, *oldvp;
	int length;
	char *swapname;
	auto int lswap;
	auto pgno_t vswap, nswap;
	auto off_t nblks;
	int s;

	switch (uap->sc_cmd) {
	case SC_GETRESVSWAP:
	case SC_GETLSWAPTOT:
	case SC_GETFREESWAP:
	case SC_GETSWAPTOT:
	case SC_GETSWAPMAX:
	case SC_GETSWAPVIRT:
		switch (uap->sc_cmd) {
		case SC_GETRESVSWAP:
			/*
			 * take maximum logical space and subtract off the
			 * amount of virtual space left.
			 * This of course can be greater than the amount
			 * of physical swap.
			 */
		        getmaxswap(&nswap, &vswap, NULL);
			nswap = maxmem + nswap + vswap - tune.t_minasmem; 
			nswap = (signed)nswap - (signed)GLOBAL_AVAILSMEM();
			break;
		case SC_GETLSWAPTOT:
			/* return total logical swap possible
			 * (sum of physical memory plus max physical swap plus
			 * virtual swap)
			 */
			getmaxswap(&nswap, &vswap, NULL);
			nswap = maxmem + nswap + vswap - tune.t_minasmem;
			break;
		case SC_GETFREESWAP:
			s = swaplock();
			nswap = _nfreeswap();
			swapunlock(s);
			break;
		case SC_GETSWAPTOT:
			getmaxswap(NULL, NULL, &nswap);
			break;
		case SC_GETSWAPMAX:
			getmaxswap(&nswap, NULL, NULL);
			break;
		case SC_GETSWAPVIRT:
			getmaxswap(NULL, &nswap, NULL);
			break;
		}
		nblks = ptod(nswap);
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
		if (ABI_IS_IRIX5_64(get_current_abi())) {
			if (copyout(&nblks, uap->sc_arg, sizeof(nblks)))
				return EFAULT;
		} else 
#endif
		     if (ABI_IS_IRIX5_N32(get_current_abi())) {
			if (copyout(&nblks, uap->sc_arg, sizeof(nblks)))
				return EFAULT;
                } else {
			irix5_off_t i5_nblks;

			i5_nblks = (irix5_off_t)nblks;
			if (copyout(&i5_nblks, uap->sc_arg, sizeof(i5_nblks)))
				return EFAULT;
		}
		return 0;
	
	case SC_GETNSWP:
		/*
		 * This data may be stale if it is ever passed back
		 * in a subsequent SC_LIST request.
		 */
		rvp->r_val1 = nswapfiles;
		return(0);
	case SC_LIST:
		{
		int i, nswapfiles_snap;
		register int cnt;
		swapinfo_t *si_snap, *st, *sip;
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
		swapent_t *ust;
#endif
		irix5_n32_swapent_t *i5_n32_ust;
		irix5_swapent_t *i5_ust;

		if (copyin(uap->sc_arg, &length, sizeof(int)))
			return(EFAULT);

retry:
		nswapfiles_snap = nswapfiles;
		if (nswapfiles_snap == 0) {
			rvp->r_val1 = 0;
			return(0);
		}

		si_snap = kmem_alloc(nswapfiles_snap * sizeof(swapinfo_t), 
						   KM_SLEEP);

		swapmodlock();

		/*
		 * If more files were added while we blocked, free our
		 * old memory reservation and try again. We want to give
		 * as accurate a picture as possible.
		 */
		if (nswapfiles != nswapfiles_snap) {
			swapmodunlock();
			kmem_free(si_snap, 
				  sizeof(swapinfo_t) * nswapfiles_snap);
			goto retry;
		}

		/*
		 * Return an error if we don't have enough space
		 * for the whole table. 
		 */
		if (length < nswapfiles) {
			swapmodunlock();
			kmem_free(si_snap, 
				  sizeof(swapinfo_t) * nswapfiles_snap);
			return(ENOMEM);
		}

		/*
		 * OK, we have enough room to copy the entire table.
		 * Skip any entries which are only in the process
		 * of being added.
		 */
		sip = si_snap;
		for (i = 0; i < NSWAPPRI; i++) {
			for (st = swaptab[i].sw_list; st; st = st->st_list) {
				if (!(st->st_flags & ST_NOTREADY)) {
					*sip = *st;
					sip++;
				}
			}
		}

		swapmodunlock();

		/*
		 * Shuffle out our local copy of the table to user mode
		 */
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
		if (ABI_IS_IRIX5_64(get_current_abi()))
			ust = (swapent_t *)((swaptbl_t *)uap->sc_arg)->swt_ent;
		else
#endif
		     if (ABI_IS_IRIX5_N32(get_current_abi()))
			i5_n32_ust = ((irix5_n32_swaptbl_t *)uap->sc_arg)->swt_ent;
		else
			i5_ust = ((irix5_swaptbl_t *)uap->sc_arg)->swt_ent;

		error = 0;
		cnt = 0;

		for (--sip; sip >= si_snap; --sip, cnt++) {
			/*
			 * We copyin the user's structure first to get
			 * a hold of the ste_path pointer which we need
			 * to copyout si_name to.
			 */
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
			if (ABI_IS_IRIX5_64(get_current_abi())) {
				swapent_t se;

				if (copyin(ust, &se, sizeof(swapent_t))) {
					error = EFAULT;
					break;
				}

				se.ste_length = sip->st_length;
				se.ste_start = sip->st_start;

				se.ste_pages = sip->st_npgs;
				se.ste_free = sip->st_nfpgs;
				se.ste_flags = sip->st_flags;
				se.ste_pri = sip->st_pri;
				se.ste_maxpages = sip->st_maxpgs;
				se.ste_vpages = sip->st_vpgs;
				se.ste_lswap = sip->st_lswap;

				if (copyout(&se, ust, sizeof(swapent_t))) {
					error = EFAULT;
					break;
				}

				/*
				 * Copyout st_name.
				 */
				ASSERT(sip->st_name != NULL);
				if (copyout(sip->st_name, se.ste_path,
			     		      (strlen(sip->st_name) + 1))) {
					error = EFAULT;
					break;
				}

				ust++;
			} else
#endif
			       if (ABI_IS_IRIX5_N32(get_current_abi())) {
				irix5_n32_swapent_t i5_n32_se;

				if (copyin(i5_n32_ust, &i5_n32_se,
						sizeof(irix5_n32_swapent_t))) {
					error = EFAULT;
					break;
				}
				i5_n32_se.ste_length =
						(irix5_n32_off_t)sip->st_length;
				i5_n32_se.ste_start =
						(irix5_n32_off_t)sip->st_start;

				i5_n32_se.ste_pages =
						(app32_long_t)sip->st_npgs;
				i5_n32_se.ste_free =
						(app32_long_t)sip->st_nfpgs;
				i5_n32_se.ste_flags =
						(app32_long_t)sip->st_flags;
				i5_n32_se.ste_pri = sip->st_pri;
				i5_n32_se.ste_maxpages =
						(app32_long_t)sip->st_maxpgs;
				i5_n32_se.ste_vpages =
						(app32_long_t)sip->st_vpgs;
				i5_n32_se.ste_lswap = sip->st_lswap;

				if (copyout(&i5_n32_se, i5_n32_ust,
						sizeof(irix5_n32_swapent_t))) {
					error = EFAULT;
					break;
				}

				/*
				 * Copyout st_name.
				 */
				ASSERT(sip->st_name != NULL);
				if (copyout(sip->st_name,
					  (char *)(__psint_t)i5_n32_se.ste_path,
			     		  (strlen(sip->st_name) + 1))) {
					error = EFAULT;
					break;
				}

				i5_n32_ust++;
			} else {
				irix5_swapent_t i5_se;

				if (copyin(i5_ust, &i5_se,
						sizeof(irix5_swapent_t))) {
					error = EFAULT;
					break;
				}
				i5_se.ste_length = (irix5_off_t)sip->st_length;
				i5_se.ste_start = (irix5_off_t)sip->st_start;

				i5_se.ste_pages = (app32_long_t)sip->st_npgs;
				i5_se.ste_free = (app32_long_t)sip->st_nfpgs;
				i5_se.ste_flags = (app32_long_t)sip->st_flags;
				i5_se.ste_pri = sip->st_pri;
				i5_se.ste_maxpages =
						(app32_long_t)sip->st_maxpgs;
				i5_se.ste_vpages = (app32_long_t)sip->st_vpgs;
				i5_se.ste_lswap = sip->st_lswap;

				if (copyout(&i5_se, i5_ust,
						sizeof(irix5_swapent_t))) {
					error = EFAULT;
					break;
				}

				/*
				 * Copyout st_name.
				 */
				ASSERT(sip->st_name != NULL);
				if (copyout(sip->st_name,
					    (char *)(__psint_t)i5_se.ste_path,
			     		    (strlen(sip->st_name) + 1))) {
					error = EFAULT;
					break;
				}

				i5_ust++;
			}
		} 

		rvp->r_val1 = cnt;
		kmem_free(si_snap, nswapfiles_snap * sizeof(swapinfo_t));

		return (error);
		}

	case SC_SGIADD:
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
		if (ABI_IS_IRIX5_64(get_current_abi())) {
			if (copyin(uap->sc_arg, &xsr, sizeof(xswapres_t)))
				return(EFAULT);
		} else
#endif
		       if (ABI_IS_IRIX5_N32(get_current_abi())) {
			irix5_n32_xswapres_t i5_n32_xsr;

			if (copyin(uap->sc_arg, &i5_n32_xsr,
						sizeof(irix5_n32_xswapres_t)))
				return(EFAULT);
			xsr.sr_name = (char *)(__psint_t)i5_n32_xsr.sr_name;
			xsr.sr_start = (off_t)i5_n32_xsr.sr_start;
			xsr.sr_length = (off_t)i5_n32_xsr.sr_length;
			xsr.sr_maxlength = (off_t)i5_n32_xsr.sr_maxlength;
			xsr.sr_vlength = (off_t)i5_n32_xsr.sr_vlength;
			xsr.sr_pri = i5_n32_xsr.sr_pri;
		} else {
			irix5_xswapres_t i5_xsr;
			if (copyin(uap->sc_arg, &i5_xsr,
						sizeof(irix5_xswapres_t)))
				return(EFAULT);
			xsr.sr_name = (char *)(__psint_t)i5_xsr.sr_name;
			xsr.sr_start = (off_t)i5_xsr.sr_start;
			xsr.sr_length = (off_t)i5_xsr.sr_length;
			xsr.sr_maxlength = (off_t)i5_xsr.sr_maxlength;
			xsr.sr_vlength = (off_t)i5_xsr.sr_vlength;
			xsr.sr_pri = i5_xsr.sr_pri;
		}
		break;
	case SC_ADD:
	case SC_REMOVE:
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
		if (ABI_IS_IRIX5_64(get_current_abi())) {
			swapres_t sr;

			if (copyin(uap->sc_arg, &sr, sizeof(swapres_t)))
				return(EFAULT);

			xsr.sr_name = sr.sr_name;
			xsr.sr_start = sr.sr_start;
			xsr.sr_length = sr.sr_length;
			xsr.sr_maxlength = sr.sr_length;
			xsr.sr_vlength = sr.sr_length;
			xsr.sr_pri = -1;
		} else
#endif
		       if (ABI_IS_IRIX5_N32(get_current_abi())) {
			irix5_n32_swapres_t i5_n32_sr;

			if (copyin(uap->sc_arg, &i5_n32_sr,
						sizeof(irix5_n32_swapres_t)))
				return(EFAULT);
			xsr.sr_name = (char *)(__psint_t)i5_n32_sr.sr_name;
			xsr.sr_start = (off_t)i5_n32_sr.sr_start;
			xsr.sr_length = (off_t)i5_n32_sr.sr_length;
			xsr.sr_maxlength = (off_t)i5_n32_sr.sr_length;
			xsr.sr_vlength = (off_t)i5_n32_sr.sr_length;
			xsr.sr_pri = -1;
		} else {
			irix5_swapres_t i5_sr;

			if (copyin(uap->sc_arg, &i5_sr,
						sizeof(irix5_swapres_t)))
				return(EFAULT);
			xsr.sr_name = (char *)(__psint_t)i5_sr.sr_name;
			xsr.sr_start = (off_t)i5_sr.sr_start;
			xsr.sr_length = (off_t)i5_sr.sr_length;
			xsr.sr_maxlength = (off_t)i5_sr.sr_length;
			xsr.sr_vlength = (off_t)i5_sr.sr_length;
			xsr.sr_pri = -1;
		}
		break;

	case SC_KSGIADD:
		if (!_CAP_ABLE(CAP_SWAP_MGT))
			return(EPERM);
		xsr = *(xswapres_t *)uap->sc_arg;
		break;
	case SC_LREMOVE:	/* delete via lswap handle */
		{
		__psint_t lswap = (__psint_t)uap->sc_arg;
		swapinfo_t *st;

		if (!_CAP_ABLE(CAP_SWAP_MGT))
			return(EPERM);
		if (lswap >= MAXLSWAP)
			return EINVAL;
		swapmodlock();
		error = 0;
		if (((st = lswaptab[lswap]) == NULL) ||
				(st->st_flags & (ST_INDEL|ST_NOTREADY)))
			error = EINVAL;
		if (!error)
			error = doswapdel(st);
		swapmodunlock();
			
		return error;
		}
	default:
		return(EINVAL);
	}

	if (!_CAP_ABLE(CAP_SWAP_MGT))
		return(EPERM);

	/* 
	 * Allocate the space to read in pathname.
	 * Note we don't wait for memory and take the
	 * chance that if we are short this request will
	 * wedge. 
	 */
	if ((swapname = (char *)kmem_alloc(MAXPATHLEN, KM_NOSLEEP)) == NULL)
		return(ENOMEM);

	if (uap->sc_cmd == SC_KSGIADD)
		error = copystr(xsr.sr_name, swapname, MAXPATHLEN, 0);
	else
		error = copyinstr(xsr.sr_name, swapname, MAXPATHLEN, 0);
	if (error)
		goto out;

	error = lookupname(swapname, UIO_SYSSPACE, FOLLOW, NULLVPP, &vp, NULL);
	if (error)
		goto out;

	/* 
	 * XXX check that if swapvp == rootvp that swplo is set??
	 */

	if (vp->v_flag & VNOSWAP) {
		VN_RELE(vp);
		error = ENOSYS;
		goto out;
	}

	/* 
	 * Check that the file is mappable - via a zero length mapping.
	 * If a new vp is returned, then it has a reference that is ours.
	 */
	oldvp = vp;
	VOP_MAP(vp, 0, 0, PROT_WRITE, MAP_PRIVATE, get_current_cred(), &vp, 
		error);
	if (error) {
		VN_RELE(oldvp);
		if (error == ENODEV)
			error = ENOSYS;	/* to comply with swapctl() man page */
		return error;
	}
	if (oldvp != vp)
		VN_RELE(oldvp);		/* release the old reference */

	switch (vp->v_type) {
	case VBLK:
		break;
	case VREG:
		VOP_ACCESS(vp, VREAD|VWRITE, get_current_cred(), error);
		break;
	case VDIR:
		error = EISDIR;
		break;
	default:
		error = ENOSYS;
		break;
	}

	if (error == 0) {
		if (uap->sc_cmd == SC_REMOVE)
			error = swapdel(vp, xsr.sr_start);
		else {
			error = swapadd(vp, &xsr, swapname, &lswap);
			if (error == 0 && uap->sc_cmd == SC_KSGIADD)
				rvp->r_val1 = lswap;
		}
		/* VN_RELE handled by swapadd/swapdel */
	} else 
		VN_RELE(vp);

out:
	kmem_free(swapname, MAXPATHLEN);
	return(error);
}	/* swapctl */

/*
 * Add first swap file
 */
int
addkswap(char *path, off_t start, off_t length, vnode_t **vp)
{
	struct swapcmda uap;
	rval_t rvp;
	xswapres_t xsr;
	int error;

#if CELL_IRIX
	char	pn[32];

	sprintf(pn,"/swap.cell.%d", cellid());
	path = pn;
	start = 0;
	length = -1;
	printf("addkswap: %s \n", path);
#endif
	xsr.sr_name = path;
	xsr.sr_start = start;
	xsr.sr_length = (length <= 0) ? -1 : length;
	xsr.sr_maxlength = -1;
	xsr.sr_vlength = -1;
	xsr.sr_pri = -1;
	uap.sc_cmd = SC_KSGIADD;
	uap.sc_arg = &xsr;
	if (error = swapctl(&uap, &rvp)) {
		return error;
	}
	*vp = lswaptab[rvp.r_val1]->st_vp;
	lswaptab[rvp.r_val1]->st_flags |= ST_BOOTSWAP;
	return 0;
}

/*
 * Add a new swap file.
 */
int
swapadd(struct vnode *vp,		/* swap file vnode */
	xswapres_t *xsr,		/* parameters of swap file */
	char *swapname,			/* name of swap file */
	int *plswap)
{
	register swapinfo_t *st, *nst = NULL;
	register pgno_t maxpgs;
	register int i, lswap;
	off_t start, end;
	int s, error;
	int locktoken;
	/*REFERENCED*/
	int unused;
	struct vattr vattr;
	int local = 0;
	vfssw_t *efs_vfs, *nfs_vfs, *specfs_vfs, *xfs_vfs;
	daddr_t swap_size;
	cred_t *crp = get_current_cred();
	extern int xfs_swappable(bhv_desc_t*);
	vnode_t *openvp;
	bhv_desc_t *bdp;

	/*
	 * some checks below are file system specific - find
	 * vfs info now
	 */
	efs_vfs = vfs_getvfssw("efs");
	nfs_vfs = vfs_getvfssw("nfs");
	specfs_vfs = vfs_getvfssw("specfs");
	xfs_vfs = vfs_getvfssw("xfs");

	/*
	 * if doesn't have a priority, assign one:
	 * BLK devices all get pri 0
	 * efs gets 2
	 * nfs gets 4
	 */
	if (xsr->sr_pri < 0) {
		bdp = vn_bhv_base_unlocked(VN_BHV_HEAD(vp));
		if (vp->v_type == VBLK)
			xsr->sr_pri = 0;
		else if (nfs_vfs && 
			((struct vnodeops *)BHV_OPS(bdp) ==
			 nfs_vfs->vsw_vnodeops))
			xsr->sr_pri = 4;
		else
			xsr->sr_pri = 2;
	} else if (xsr->sr_pri >= NSWAPPRI) {
		VN_RELE(vp);
		return EINVAL;
	}


	/* Open the swap file - might change vp .. */
	openvp = vp;
	VOP_OPEN(openvp, &vp, FREAD|FWRITE, crp, error);
	if (error)
		return error;
	bdp = vn_bhv_base_unlocked(VN_BHV_HEAD(vp));

	/* determine if local */
	if (efs_vfs &&
	    efs_vfs->vsw_vnodeops == (struct vnodeops *)BHV_OPS(bdp))
		local = 1;
	if (xfs_vfs &&
	    xfs_vfs->vsw_vnodeops == (struct vnodeops *)BHV_OPS(bdp))
		local = 1;
	if (specfs_vfs &&
	    specfs_vfs->vsw_vnodeops == (struct vnodeops *)BHV_OPS(bdp))
		local = 1;

	/* only do 1 add/delete at a time */
	swapmodlock();

	/* compute size of resource */
	if (vp->v_type == VREG) {
		struct bmapval iex;

		vattr.va_mask = AT_SIZE;
		VOP_GETATTR(vp, &vattr, 0, crp, error);
		if (error)
			goto bad;
		if (xfs_vfs &&
		    (xfs_vfs->vsw_vnodeops ==
		     (struct vnodeops *)BHV_OPS(bdp))) {
			error = xfs_swappable(bdp);
			if (error) {
				goto bad;
			}
		}
		/*
		 * kind of kludge - must get indirect extents in core -
		 * easiest way to do this is to call bmap
		 */
		if (vattr.va_size) {
			s = 1;
			VOP_RWLOCK(vp, VRWLOCK_WRITE);
			VOP_BMAP(vp, vattr.va_size-1, 1, B_READ, 
					crp, &iex, &s, error);	
			if (error) {
				VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
				goto bad;
			}
			VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
		}
		swap_size = vattr.va_size >> SCTRSHFT;

	} else {
		/* stolen from specvnops.c */
		struct bdevsw *my_bdevsw;

		my_bdevsw = get_bdevsw(vp->v_rdev);
		if (!bdstatic(my_bdevsw)) {
			error = ENXIO;
			goto bad;
		}
		if ((int (*)(void))my_bdevsw->d_size64 != nulldev) {
			error = (*my_bdevsw->d_size64)(vp->v_rdev, &swap_size);
			if (error)
				goto bad;
		} else if ((int (*)(void))my_bdevsw->d_size != nulldev)
			swap_size = (*my_bdevsw->d_size)(vp->v_rdev);
		else {
			error = ENXIO;
			goto bad;
		}
	}
	if (swap_size < xsr->sr_start) {
		error = EINVAL;
		goto bad;
	}

	/*
	 * Do defaults/sanity checking for length, maxlength, vlength
	 */
	if (xsr->sr_length == -1)
		xsr->sr_length = swap_size - xsr->sr_start;
	if (xsr->sr_maxlength == -1)
		xsr->sr_maxlength = xsr->sr_length;
	if (xsr->sr_vlength == -1)
		xsr->sr_vlength = xsr->sr_maxlength;

	maxpgs = dtopt(xsr->sr_maxlength);
	if (xsr->sr_maxlength < xsr->sr_length) {
		error = EINVAL;
		goto bad;
	}
		
	if (vp->v_type == VBLK && xsr->sr_maxlength != xsr->sr_length) {
		/* block devices do not grow */
		error = EINVAL;
		goto bad;
	}
	/* virtual length must be equal or larger than other lengths */
	if (xsr->sr_vlength < xsr->sr_length ||
	    xsr->sr_vlength < xsr->sr_maxlength) {
		error = EINVAL;
		goto bad;
	}

	/*
	 * XXX do not permit growing yet - if we do, then must
	 * handle sometime syncing meta-data associated with file out to
	 * disk ... currently VOP_SYNC skips ISSWAP files
	 * Also must watch losing buf that holds swapvp - then
	 * the iupdate may require memory to sync ..
	 */
	if (xsr->sr_length != xsr->sr_maxlength) {
		error = EINVAL;
		goto bad;
	}
	if (swap_size < (xsr->sr_start + xsr->sr_length)) {
		error = EINVAL;
		goto bad;
	}
	if (vp->v_flag & VISSWAP) {
		error = EBUSY;
		goto bad1;
	}

	VN_FLAGSET(vp, VISSWAP);

	/*
	 * we need to be sure that no pages of this file are in the
	 * page cache since as soon as we start to swap the cache tags
	 * will be different (instead of vp it will be anon ptr).
	 * In addition we have set the V_ISSWAP flag to notify others
	 * that this file CANNOT be opened.
	 * Call VOP_FSYNC to make sure the swap file meta-data are properly
	 * syncd to disk
	 */
	VOP_FSYNC(vp, FSYNC_WAIT|FSYNC_INVAL, crp, (off_t)0, (off_t)-1,error);
	if (error)
		goto bad;

	/*
	 * allocate a new swapinfo struct, bitmap, and string space
	 */
	nst = kmem_zalloc(sizeof(*nst), KM_SLEEP);
	nst->st_name = kmem_alloc(strlen(swapname) + 1, KM_SLEEP);
	nst->st_bmap = kmem_zalloc((maxpgs / 8) + 1, KM_SLEEP);
#ifdef SWAPDEBUG
	nst->st_cksum = kmem_zalloc(maxpgs * sizeof(long), KM_SLEEP);
#endif

	/*
	 * find free logical swap id - don't use 0 since NULL sm_swaphandle
	 * is 0
	 */
	for (lswap = 1; lswap < MAXLSWAP; lswap++)
		if (lswaptab[lswap] == NULL)
			break;
	if (lswap >= MAXLSWAP) {
		error = ENOSPC;
		goto bad;
	}

	/* check for overlaps with existing swap files */
	start = xsr->sr_start;
	end = start + xsr->sr_length;
	for (i = 0; i < NSWAPPRI; i++) {
		for (st = swaptab[i].sw_list; st; st = st->st_list) {
			if (vp != st->st_vp ||
			    ((start >= (st->st_start + st->st_length)) &&
			    (end <= st->st_start)))
				continue;
			error = EEXIST;
			goto bad;
		}
	}

	/* Initialize the new entry */
	nst->st_vp = vp;
	nst->st_flags = ST_NOTREADY | (local ? ST_LOCAL_SWAP : 0);
	nst->st_pri = xsr->sr_pri;
	nst->st_lswap = lswap;
	*plswap = nst->st_lswap;
	nst->st_start = xsr->sr_start;
	nst->st_length = xsr->sr_length;
	nst->st_swppglo = dtop(xsr->sr_start); /* round up */
	nst->st_npgs = dtopt(xsr->sr_length);
	nst->st_nfpgs = nst->st_npgs;
	nst->st_maxpgs = maxpgs;
	nst->st_allocs = 0;
	nst->st_gen = 0;

	/*
	 * record vpgs as the actual number of 'virtual' pages - those
	 * over and above any 'real physical swap pages
	 */
	nst->st_vpgs = dtopt(xsr->sr_vlength) - maxpgs;
	bcopy(swapname, nst->st_name, strlen(swapname) + 1);

	/* add to logical swap table && priority list */
	locktoken = swaplock();
	lswaptab[lswap] = nst;
	nst->st_list = swaptab[nst->st_pri].sw_list;
	swaptab[nst->st_pri].sw_list = nst;

	if (swaptab[nst->st_pri].sw_next == NULL)
		swaptab[nst->st_pri].sw_next = nst;

	nswapfiles++;
	swapunlock(locktoken);

	/* Add the swap space to the total available space count. */
	reservemem(GLOBAL_POOL, -(nst->st_vpgs + maxpgs), 0, 0);

	/* Clearing the flags allows sm_alloc to find it */
	nst->st_flags &= ~ST_NOTREADY;

	swapmodunlock();
	return 0;

bad:
#ifdef SWAPDEBUG
	if (nst && nst->st_cksum)
		kmem_free(nst->st_cksum, maxpgs * sizeof(long));
#endif
	if (nst && nst->st_bmap)
		kmem_free(nst->st_bmap, (maxpgs / 8) + 1);
	if (nst && nst->st_name)
		kmem_free(nst->st_name, strlen(swapname) + 1);
	if (nst)
		kmem_free(nst, sizeof(*nst));
	/* we opened file, so we can't really have any file locks against it */
	VN_FLAGCLR(vp, VISSWAP);
bad1:
	VOP_CLOSE(vp, FREAD|FWRITE, L_TRUE, crp, unused);
	VN_RELE(vp);			/* one for lookup now */
	swapmodunlock();

	return error;
}

#ifdef DELDEBUG
int nxswap;
int nnotdone;
int npfnnonzero;
int nswaperr;
int nnomem;
int nnoshdlck;
int nkv;
int nnoswap;
int nswpchg;

static void
prdelstats(void)
{
	printf("swapdel:xswap %d notdone %d pfnnon0 %d swaperr %d nswpchg %d\n",
		nxswap, nnotdone, npfnnonzero, nswaperr, nswpchg);
	printf(" nomem %d noshdlck %d nkv %d nnoswap %d\n",
		nnomem, nnoshdlck, nkv, nnoswap);
}

static void
initdelstats(void)
{
	nxswap = nnotdone = npfnnonzero = nswaperr =
		nnomem = nnoshdlck = nkv = nnoswap = nswpchg = 0;
}
#endif

/*
 * Delete a swap file - based on vp and start.
 */
int
swapdel(struct vnode *vp, off_t start)
{
	register swapinfo_t *st;
	int i, error;

	/* only do 1 add/delete at a time */
	swapmodlock();

	/*
	 * Find the swap file table entry for the file to
	 * be deleted.
	 * In progress adds and deletes are ignored (i.e. they give errors)
	 */

	for (i = 0; i < NSWAPPRI; i++) {
		for (st = swaptab[i].sw_list; st; st = st->st_list) {
			if (!(st->st_flags & (ST_INDEL|ST_NOTREADY)))
				if (st->st_vp == vp && st->st_start == start)
					break;
		}
		if (st)
			break;
	}
	if (!st) {
		swapmodunlock();
		return EINVAL;
	}
	error = doswapdel(st);
	swapmodunlock();
	VN_RELE(vp);		/* remove reference from lookup */
	return error;
}

#if DEBUG
int swapdel_waited;
#endif

/*
 * Delete a swap file.
 * Can fail with ENOMEM if there are too few free pages to swap in all
 * the swap pages, or if it will exceed some miser job's rss limit.
 */
static int
doswapdel(swapinfo_t *st)
{
	register swapinfo_t *pst, *tst;
	swaptab_t *tabp;
	register int passes;
	pgno_t maxpgs, rpgs;
	int ret;
	int s;
	vnode_t *vp = st->st_vp;
	as_scan_t scanargs;
	struct delarg da;

	/* once tagged with INDEL, noone will look at it */
	s = swaplock();
	st->st_flags |= ST_INDEL;
	swapunlock(s);

	rpgs = st->st_vpgs + st->st_maxpgs;
	if (reservemem(GLOBAL_POOL, rpgs, 0, 0)) {
		st->st_flags &= ~ST_INDEL;	/* still good! */
		cmn_err(CE_NOTE, "swapdelete - too few free pages");
		return ENOMEM;
	}

#ifdef DELDEBUG
	initdelstats();
#endif

	/* first get rid of any unecessary backing pages */
	scanargs.as_scan_shake.as_shakeswap_lswap = st->st_lswap;
	scanargs.as_scan_shake.as_shakeswap_hard = 0;
	if (as_scan(AS_SWAPSCAN, 0, &scanargs) == EMISER) {
		st->st_flags &= ~ST_INDEL;      /* still good! */
		cmn_err(CE_NOTE, "swapdelete - miser job limit reached");
		unreservemem(GLOBAL_POOL, rpgs, 0, 0);
		return ENOMEM;
	}
#ifdef DELDEBUG
	prdelstats();
	initdelstats();
#endif

	/*
	 * do a easy check to try to make sure that system won't totally
	 * go to pieces if we remove this swap device
	 * Is amount of swap we are currently using on the device greater
	 * than the amount of places (main memory + other swap devices)?
	 */
	if ((st->st_npgs - st->st_nfpgs) > (GLOBAL_FREEMEM() + _nfreeswap())) {
		st->st_flags &= ~ST_INDEL;
		unreservemem(GLOBAL_POOL, rpgs, 0, 0);
		return ENOMEM;
	}

	/* get back pages from sysreg */
	unswapsysreg();

	/* reclaim pages */
	passes = 0;
	while (st->st_nfpgs < st->st_npgs) {
		if (passes > 10) {
			st->st_flags &= ~ST_INDEL;
			unreservemem(GLOBAL_POOL, rpgs, 0, 0);
			return EBUSY;
		}
		scanargs.as_scan_shake.as_shakeswap_lswap = st->st_lswap;
		scanargs.as_scan_shake.as_shakeswap_hard = 1;
		ret = as_scan(AS_SWAPSCAN, 0, &scanargs);
		if (ret == EMISER) {
			st->st_flags &= ~ST_INDEL;
			unreservemem(GLOBAL_POOL, rpgs, 0, 0);
			return EBUSY;
		} else if (ret) {
			/* ran out of memory! - go sxbrk and try again */
			setsxbrk();
		} else {
			delay(10);
			passes++;
		}
		da.lswap = st->st_lswap;
		da.hard = 1;
		unswapshm(&da);
#ifdef DELDEBUG
		prdelstats();
		initdelstats();
#endif
	}

	/*
	 * Someone might have been in sm_alloc doing the bit map search
	 * while we started deleting the swap device.  They should be
	 * out of there by now, but check here just in case.
	 */

	if (st->st_allocs != 0) {
		cmn_err(CE_WARN, "st_allocs not zero at end of swap deletion.  Waiting...");
#if DEBUG
		swapdel_waited++;
#endif

		while (st->st_allocs != 0)
			delay(HZ);
	}

	/* all done */
	s = swaplock();
	ASSERT(nswapfiles > 0);
	ASSERT(lswaptab[st->st_lswap] == st);

	nswapfiles--;
	lswaptab[st->st_lswap] = NULL;
	tabp = &swaptab[st->st_pri];
	for (pst = NULL, tst = tabp->sw_list; tst;
					pst = tst, tst = tst->st_list) {
		if (tst == st) {
			if (pst)
				pst->st_list = st->st_list;
			else
				tabp->sw_list = st->st_list;
			break;
		}
	}
	tabp->sw_next = tabp->sw_list;
	ASSERT(tst);
	swapunlock(s);

	maxpgs = st->st_maxpgs;
	if (st->st_cksum)
		kmem_free(st->st_cksum, maxpgs * sizeof(long));
	if (st->st_bmap)
		kmem_free(st->st_bmap, (maxpgs / 8) + 1);
	if (st->st_name)
		kmem_free(st->st_name, strlen(st->st_name) + 1);
	kmem_free(st, sizeof(*st));

	VOP_CLOSE(vp, FREAD|FWRITE, L_TRUE, get_current_cred(), ret);
	VN_FLAGCLR(vp, VISSWAP);
	VN_RELE(vp);	/* we held one while a swap file */

	return 0;
}

/*
 * free up swap space associated with shared memory segments.
 *
 * Loop through all shared memory segments, and unswap the regions
 * associated with them.
 */
static void
unswapshm(struct delarg *da)
{
	vshm_iterate(do_unswapreg, da);
}

void
do_unswapreg(vshm_t *vshm, void	*arg)
{
	struct delarg	*da;
	struct region	*rp;
	as_mohandle_t mo;

	VSHM_GETMO(vshm, &mo);
	rp = mo.as_mohandle;

	if (!rp)
		return;

	da = (struct delarg *)arg;

	reglock(rp);
	if (!unswapreg(0, rp, da->hard, da->lswap, 0))
		regrele(rp);
}
	
/*
 * Pull the region back from the swap disk.  If we know where to put it
 * in memory, try to leave it there.  Otherwise, just exchange swap
 * handles.
 */
int
unswapreg(pas_t *pas, reg_t *rp, int hard, int lswap, preg_t *prp)
{
	register pgno_t rpn, apn, spn, epn;
	sm_swaphandle_t sh;
	swaphandle_t sw;
	void *id;
	int result, errs = 0;

	if (!(rp->r_flags & RG_ANON)) {
		ASSERT(rp->r_anon == NULL);
		return 0;
	}

	(void) anon_shake_tree(rp->r_anon);
	
	/*
	 * Set up the start and end page numbers based on the pregion
	 * that we're provided.  It's possible that the pregion is only
	 * mapping part of the region so we need to be careful how
	 * much we unmap.  If no pregion is provided, we assume that we
	 * will need to unmap the entire region.
	 */
	if (prp) {
		spn = prp->p_offset;
		epn = prp->p_pglen;
		ASSERT(pas);
	} else {
		spn = 0;
		epn = rp->r_pgsz;
		ASSERT(pas == 0);
	}

	for (rpn = spn; rpn < epn; rpn++) {
		apn = prp ? rpntoapn(prp, rpn) : rpn;
		if ((id = anon_lookup(rp->r_anon, apn, &sh)) != NULL) {
			if (sh) {
				sw.sm_all = sh;
				if (sw.sm_lswap != lswap)
					continue;
				if (prp == NULL ||
				   (rp->r_anon != (anon_hdl)id &&
				   !anon_isdegenerate(rp->r_anon)))
					result = xswap(apn, prp, rp, hard);
				else
					result = unswap(pas, rpn, prp, hard);

				switch(result)
				{
				case  0: /* success, keep going */
					break;
				case -1: /* out of memory, stop here */
					return 1;
				case EMISER:
					/* Only possible in the shake case */
					ASSERT(prp && pas);
					return(EMISER);
				default: /* random error, relock and continue */
					/* well if we get to many, let's
					 * not get into an infinite loop
					 */
					if (++errs > 100)
						return ENOMEM;
					reglock(rp);
					break;
				}
			}
		}
	}
	return 0;
}

/*
 * Swap space shaker.  This is called when we run out of swap space
 * to try and recover some unused space from the anonymous manager.
 * At this point, the algorithm is all or nothing -- it runs through
 * all address spaces and all anon trees.  it never sleeps on an address
 * space lock.
 */
/* ARGSUSED */
static int
shake_swap(int level)
{
	ASSERT(level == SHAKEMGR_SWAP);

	if (mutex_trylock(&shake_swap_lock) == 0)
		return(0);

	/* XXX should this be local only?? */
	as_scan(AS_ANONSCAN, 0, NULL);
	mutex_unlock(&shake_swap_lock);
	return(0);
}

/*
 * unswap - attempt to release a single page of swap
 * if things get difficult, we just return and hope that next
 * time life will be easier
 *
 * Returns - 0 if cleared swap w/o releasing region lock or just skipping page
 *	     -1 if no mem
 *	     -2/EMISER indicates miser job's limit would be exceeded
 *	     >0 error
 * XXX sysreg??
 * Releases region lock unless returns 0
 */
int traceunswap = 0;
int traceunswapall = 0;

static int
unswap(pas_t *pas, pgno_t rpn, preg_t *prp, int hard)
{
	pfd_t *pfd;
	pfd_t *swappfd = NULL;
	pglst_t swaplst[1];
	reg_t *rp = prp->p_reg;
	uint cachekey;
	pgno_t apn = rpntoapn(prp, rpn);
	caddr_t vaddr;
	/*REFERENCED*/
        attr_t	*attr;
	/*REFERENCED*/
        struct pm *pm;
	register pde_t *pd;
	sm_swaphandle_t newsh = NULL;
	auto void *id;
	auto sm_swaphandle_t sp;

	/*
	 * The only safe cases to deal with are :
	 * 1) leaf anon nodes (id == r_anon)
	 * 2) an interior node in a degenerate anon tree (ones with no branches)
	 *
	 * In either case we can guarantee that all (namely the one pregion/pmap
	 * we have locked) are referenced in a pmap. Only if they
	 * are can we remove the backing swap handle
	 */

	ASSERT(pas && prp);
	vaddr = rpntov(prp, rpn);
again:
	if ((pd = pmap_pte(pas, prp->p_pmap, vaddr, VM_NOSLEEP)) == NULL) {
		regrele(rp);
#ifdef DELDEBUG
		nnomem++;
#endif
		return -1;
	}

        /*
         * The pm we get here can be used safely because we
         * have a reference from the region prp, which is
         * locked and therefore cannot be modified.
         */
        attr = findattr(prp, vaddr);
        pm = attr_pm_get(attr);
        
	ASSERT(pg_getpgi(pd) != SHRD_SENTINEL);
	if (!pg_isvalid(pd) && pg_getpfn(pd) != 0) {
		/* someone else in process of faulting in */
#ifdef DELDEBUG
		npfnnonzero++;
#endif
		if (swappfd) {
			pagefree(swappfd);
			VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND,
							0, -1);
		}
		return(0);
	}

	if (pfd = anon_pfind(rp->r_anon, apn, &id, &sp)) {
		if (swappfd) {
			pagefree(swappfd);
			VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND,
							0, -1);
		}
		/*
		 * already in cache - interior nodes present a problem.
		 * The basic invariant that must be maintained is that either
		 * a page is referenced in ALL the pmap(s) or is out on swap.
		 * The only exception to this is if a shared region is
		 * marked HASSANON. Note that HASSANON only works for a single
		 * region since once a region is freed is removes all SANON
		 * pages.
		 */
		if (!(pfd->pf_flags & P_DONE)) {
			if (rp->r_flags & RG_HASSANON)
				pagefreesanon(pfd, 0);
			else
				anon_pagefree_and_cache(pfd);
#ifdef DELDEBUG
			nnotdone++;
			if (traceunswap && (id != rp->r_anon || traceunswapall))
				printf("int apn %d id 0x%x swap 0x%x not done\n", apn, id, sp);
#endif
			return(0);
		}
		ASSERT((pfd->pf_flags & (P_ANON|P_SWAP)) == (P_ANON|P_SWAP));
		ASSERT(pfd->pf_pchain == NULL);

		if (!pg_isvalid(pd)) {
			int error;

                        /*
                         * We need to explicitly hold the pfdat
                         * because we're calling handlepd after
                         * rmap_addmap.
                         */
			pfdat_hold(pfd);
			VPAG_UPDATE_RMAP_ADDMAP_RET(PAS_TO_VPAGG(pas),
				    JOBRSS_INC_FOR_PFD, pfd, pd, pm, error);
			if (error) {
				pfdat_release(pfd);
				if (rp->r_flags & RG_HASSANON)
					pagefreesanon(pfd, 0);
				else
					anon_pagefree_and_cache(pfd);
				return (EMISER);
			}
			anon_clrswap(id, apn, NULL);
			MINFO.cache++;

                        pg_setpfn(pd, pfdattopfn(pfd));
			pg_setccuc(pd, attr->attr_cc, attr->attr_uc);
                        prp->p_nvalid++;
			(void) handlepd(vaddr, pfd, pd, prp, 1);
                        pfdat_release(pfd);
		} else {
			anon_clrswap(id, apn, NULL);
			if (rp->r_flags & RG_HASSANON)
				pagefreesanon(pfd, 0);
			else
				anon_pagefree_and_cache(pfd);
		}
#ifdef DELDEBUG
		if (traceunswapall)
			printf("apn %d id 0x%x swap 0x%x pfd 0x%x cnt %d hashed\n",
				apn, id, sp, pfd, pfd->pf_use);
#endif
		ASSERT(pfd->pf_use >= 1);
		return(0);
	}

	if (!hard) {
		return(0);
	}

	if (swappfd == NULL) {

		/*
		 * first time through alloc page and swap in
		 */
		if (VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND, 
							0, 1)) {
			return(EMISER);
		}

		cachekey = vcache2(prp, attr, apn);
		if ((swappfd = pagealloc(cachekey, 0)) == NULL) {
			/* no memory */
			regrele(rp);
			VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND,
							0, -1);
#ifdef DELDEBUG
			nnomem++;
#endif
			return(-1);
		}

		regrele(rp);
		swaplst[0].gp_pfd = swappfd;
		ASSERT(swappfd->pf_pchain == NULL);
		if (sm_swap(swaplst, sp, 1, B_READ, NULL)) {
			pagefree(swappfd);
			VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND,
							0, -1);
#ifdef DELDEBUG
			nswaperr++;
#endif
			return(EIO);
		}
		ASSERT(swappfd->pf_pchain == NULL);

		/*
		 * now have page successfully read in - check that while things
		 * were unlocked that the page wasn't faulted in or
		 * that the anon/swap handle changed
		 * Note that region can't have gone away or changed size
		 * since we have aspacelock
		 */
		reglock(rp);
		if ((id != anon_lookup(rp->r_anon, apn, &newsh)) ||
								newsh != sp) {
			pagefree(swappfd);
			VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND,
							0, -1);
			return(0);
		}
		goto again;
	}

	/*
	 * have a replacement page!
	 */

	if (anon_swapin(id, swappfd, apn) == DUPLICATE) {
		pagefree(swappfd);
		swappfd = NULL;
		goto again;
	}

	anon_clrswap(id, apn, NULL);

	ASSERT(!pg_isvalid(pd));
	MINFO.swap++;

        /*
         * We have to explicitly hold the pfdat 
         * because we're calling handlepd after adding
         * the reverse map link.
         */
        pfdat_hold(swappfd);
        
        pg_setpfn(pd, pfdattopfn(swappfd));
	pg_setccuc(pd, attr->attr_cc, attr->attr_uc);
	prp->p_nvalid++;
	VPAG_UPDATE_RMAP_ADDMAP(PAS_TO_VPAGG(pas), JOBRSS_INS_PFD, swappfd, 
							pd, pm);
	(void) handlepd(vaddr, swappfd, pd, prp, 1);
	pagedone(swappfd);

        pfdat_release(swappfd);
        
#ifdef DELDEBUG
	if (traceunswapall)
		printf("apn %d id 0x%x swap 0x%x pfd 0x%x cnt %d unswapped\n",
				apn, id, sp, swappfd, swappfd->pf_use);
#endif
	return(0);
}

/*
 * xswap - exchange swap handles
 *
 * Since we hold the region locked, and guarantee that anon_lookup
 * on this locked region continues to point to the swap handel in question
 * we need not worry about other regions looking up and potentially
 * swapping in the same page. It does no harm to pinsert the new page
 * if we swapped it in.
 */
static int
xswap(pgno_t apn, preg_t *prp, reg_t *rp, int hard)
{
	pfd_t *pfd;
	pfd_t *swappfd = NULL;
	pglst_t swaplst[1];
	uint cachekey;
	auto sm_swaphandle_t newsh;
	auto sm_swaphandle_t tsh;
	auto int npgs;
	auto void *id;
	auto sm_swaphandle_t sp;

	if (!hard)
		return 0;

	npgs = 1;
	newsh = sm_alloc(&npgs, 0);
	if (npgs != 1) {
#ifdef DELDEBUG
		nnoswap++;
#endif
		return 0;
	}

again:
	if (pfd = anon_pfind(rp->r_anon, apn, &id, &sp)) {
		if (swappfd) {
			pagefree(swappfd);
			swappfd = NULL;
		}
		if (!(pfd->pf_flags & P_DONE)) {
			if (rp->r_flags & RG_HASSANON)
				pagefreesanon(pfd, 0);
			else
				anon_pagefree_and_cache(pfd);
			sm_free(&newsh, 1);
#ifdef DELDEBUG
			nnotdone++;
#endif
			return(0);
		}
		ASSERT((pfd->pf_flags & (P_ANON|P_SWAP)) == (P_ANON|P_SWAP));
		ASSERT(pfd->pf_pchain == NULL);
	} else if (swappfd == NULL) {
		regrele(rp);
		/*
		 * first time through alloc page and swap in
		 */
		if (prp)
			cachekey = vcache2(prp, &prp->p_attrs, apn);
		else
			cachekey = vcache(rp->r_gen, apn);
		if ((swappfd = pagealloc(cachekey, 0)) == NULL) {
			/* no memory */
			sm_free(&newsh, 1);
#ifdef DELDEBUG
			nnomem++;
#endif
			return(-1);
		}

		swaplst[0].gp_pfd = swappfd;
		ASSERT(swappfd->pf_pchain == NULL);
		if (sm_swap(swaplst, sp, 1, B_READ, NULL)) {
			pagefree(swappfd);
			sm_free(&newsh, 1);
#ifdef DELDEBUG
			nswaperr++;
#endif
			return(EIO);
		}
		ASSERT(swappfd->pf_pchain == NULL);

		/*
		 * now have page successfully read in - check that while things
		 * were unlocked that the page wasn't faulted in or
		 * that the anon/swap handle changed
		 * Note that region can't have gone away or changed size
		 * since we have aspacelock
		 */
		reglock(rp);
		if ((id != anon_lookup(rp->r_anon, apn, &tsh)) || tsh != sp) {
#ifdef DELDEBUG
			nswpchg++;
#endif
			pagefree(swappfd);
			sm_free(&newsh, 1);
			return(0);
		}
		goto again;
	}

	if (swappfd) {
		/* enter page into hash */

		if (anon_swapin(id, swappfd, apn) == DUPLICATE) {
			pagefree(swappfd);
			swappfd = NULL;
			goto again;
		}

		pagedone(swappfd);
	} else {
		swappfd = pfd;
	}

	/*
	 * At this point, swappfd has contents of page we wish to exchange
	 * swap on & newsh has a new swap handle
	 */
	regrele(rp);
	swaplst[0].gp_pfd = swappfd;
	ASSERT(swappfd->pf_pchain == NULL);
	if (sm_swap(swaplst, newsh, 1, B_WRITE, NULL)) {
		if (rp->r_flags & RG_HASSANON)
			pagefreesanon(swappfd, 0);
		else
			pagefree(swappfd);
		sm_free(&newsh, 1);
#ifdef DELDEBUG
		nswaperr++;
#endif
		return(EIO);
	}
	ASSERT(swappfd->pf_pchain == NULL);

	/* one last time - check that everything is still correct */
	reglock(rp);
	if ((id != anon_lookup(rp->r_anon, apn, &tsh)) || tsh != sp) {
#ifdef DELDEBUG
		nswpchg++;
#endif
		if (rp->r_flags & RG_HASSANON)
			pagefreesanon(swappfd, 0);
		else
			anon_pagefree_and_cache(swappfd);
		sm_free(&newsh, 1);
		return(0);
	}
	anon_clrswap(id, apn, newsh);

	if (rp->r_flags & RG_HASSANON)
		pagefreesanon(swappfd, 0);
	else
		anon_pagefree_and_cache(swappfd);

#ifdef DELDEBUG
	nxswap++;

	if (traceunswapall)
		printf("apn %d id 0x%x swap 0x%x pfd 0x%x cnt %d xswapped\n",
				apn, id, sp, swappfd, swappfd->pf_use);
#endif
	return(0);
}

static void
unswapsysreg(void)
{

}

/*
 * swapfind - find best fit
 */
static pgno_t
swapfind(swapinfo_t *sp, int size, int *fsize)
{
	bitnum_t max, beststart, start, end;
	bitlen_t len, bestlen;
	int pass2 = 0;

	/* starting at st_next, search for 'size' free bits */
	bestlen = 0;

	start = sp->st_next;
	end = (bitnum_t)sp->st_npgs;
again:
	while (start < end) {
		max = MIN(size, end - start);
		len = bftstclr(sp->st_bmap, start, max);
		ASSERT(len <= size);
		if (len == size) {
			/* found one */
			*fsize = size;
			return((pgno_t)start);
		} else if (len > bestlen) {
			beststart = start;
			bestlen = len;
		}
		/* skip over any clear bits */
		start += len;
		/* skip over set bits */
		start += bftstset(sp->st_bmap, start, max - len);
		ASSERT(start <= end);
	}

	if (!pass2) {
		pass2++;
		start = 0;
		end = sp->st_next;
		goto again;
	}
	if (bestlen) {
		/* return best we got */
		*fsize = bestlen;
		return((pgno_t)beststart);
	}

	*fsize = 0;
	return(-1);
}

/*
 * isswapdeleted - return true if swap device is going away
 */
int
isswapdeleted(sm_swaphandle_t sh)
{
	register swaphandle_t sw;

	sw.sm_all = sh;
	return(lswaptab[sw.sm_lswap]->st_flags & ST_OFFLINE);
}

/*
 * nfreeswap - return amount of freeswap (in disk blocks)
 * Note - called from interrupt level so can't grab semaphores ..
 */
int
nfreeswap(ulong *freeblocks)
{
	if (cpswapmodlock() == 0)
		return -1;
	*freeblocks = ptod(_nfreeswap());
	swapmodunlock();
	return 0;
}

/*
 * _nfreeswap - return amount of freeswap (in pages)
 * Must be called with swapmodlock set
 */
static pgno_t
_nfreeswap(void)
{
	register swapinfo_t *st;
	register pgno_t tfreeswap = 0;
	int i;

	for (i = 0; i < NSWAPPRI; i++) {
		for (st = swaptab[i].sw_list; st; st = st->st_list) {
			if (!(st->st_flags & ST_OFFLINE))
				tfreeswap += st->st_nfpgs;
		}
	}
	return tfreeswap;
}

/*
 * getmaxswap - return amount of swap (in pages)
 */
void
getmaxswap(pgno_t *max, pgno_t *vmax, pgno_t *tot)
{
	register swapinfo_t *st;
	register pgno_t curswap = 0, tswap = 0, tvswap = 0;
	int i;

	swapmodlock();
	for (i = 0; i < NSWAPPRI; i++) {
		for (st = swaptab[i].sw_list; st; st = st->st_list) {
			if (!(st->st_flags & ST_OFFLINE)) {
				tswap += st->st_maxpgs;
				tvswap += st->st_vpgs;
				curswap += st->st_npgs;
			}
		}
	}
	swapmodunlock();
	if (max)
		*max = tswap;
	if (vmax)
		*vmax = tvswap;
	if (tot)
		*tot = curswap;
}

/*
 * Swap Manager
 */

#if DEBUG
int alloc_offline, alloc_gen, alloc_out;
#endif

/*
 * sm_alloc - allocate contigous pages
 * Returns # pages allocated
 */
sm_swaphandle_t
sm_alloc(int *npgs, int local)
{
	register bitnum_t bitnum;
	register int i;
	register pgno_t	swappg;
	auto int fsize = 0;		/* found # of pages */
	swaphandle_t sw;
	swapinfo_t *st, *stop;
	int s;
	int shaken = 0;
	ushort_t gen;

try_again:
	sw.sm_all = 0;
	swappg = -1;

	/*
	 * Search, in priority order, for swap space
	 */
	s = swaplock();
	for (i = 0; i < NSWAPPRI; i++) {

		/*
		 * Start looking where we left off last time through.  We
		 * stop when we loop around the last back to where we 
		 * started from.  The stopping point will be set the
		 * first time through.
		 */

		stop = NULL;

		for (st = swaptab[i].sw_next; st != stop ; st = st->st_list) {

			/*
			 * First time through, remember our stopping point.
			 */

			if (stop == NULL)
				stop = swaptab[i].sw_next;

			/*
			 * If we hit the end of the list, then loop back to
			 * check the swap areas at the beginning of the list.
			 */

			if (st == NULL) {
				st = swaptab[i].sw_list;

				/*
				 * If we've been here already, then we're
				 * done with this list.
				 */

				if (st == stop)
					break;
			}

			if (st->st_flags & ST_OFFLINE)
				continue;

			if (local && ((st->st_flags & ST_LOCAL_SWAP) == 0))
				continue;

			/*
			 * Unlock the swaplock while we search for space
			 * since this can take awhile if the bit map is
			 * large and fragmented.  When we come back,
			 * re-check to make sure the swap device hasn't
			 * been deleted.  The st_alloc counter is used
			 * to let doswapdel() know we're using the bit
			 * map.
			 */

find_again:
			st->st_allocs++;
			gen = st->st_gen;
			swapunlock(s);

			swappg = swapfind(st, *npgs, &fsize);

			s = swaplock();
			st->st_allocs--;

			/*
			 * Swap device deleted?  Go try another one.
			 */

			if (st->st_flags & ST_OFFLINE) {
#if DEBUG
				alloc_offline++;
#endif
				continue;
			}

			/*
			 * Someone else modified the bit map while we were
			 * looking.  Redo the search in case things have
			 * changed.
			 */

			if (st->st_gen != gen) {
#if DEBUG
				alloc_gen++;
#endif
				goto find_again;
			}

			if (fsize > 0)
				/* XXX search for best??? */
				break;
		}
		if (fsize > 0)
			break;
	}

	if (fsize == 0) {
		swapunlock(s);
		if (!shaken) {
			(void) shake_shake(SHAKEMGR_SWAP);
			shaken = 1;
			goto try_again;
		}
		*npgs = 0;
#if DEBUG
		alloc_out++;
#endif
		return((sm_swaphandle_t)sw.sm_all);
	}

	ASSERT(fsize > 0);

	bitnum = (bitnum_t)swappg;

	/* set swappg to physical pageno on swap device */
	swappg = st->st_swppglo + swappg;
#ifdef _VCE_AVOIDANCE
	if (vce_avoidance) {
		/* On mapped files we limit the swap size - XXX */
		if (st->st_vp->v_type != VBLK)
			fsize = 1;
	}
#endif
	st->st_next = bitnum + fsize;
	st->st_nfpgs -= fsize;
	ASSERT(st->st_nfpgs >= 0);
	swaptab[st->st_pri].sw_next = st->st_list ? st->st_list : swaptab[st->st_pri].sw_list;

	sw.sm_lswap = st->st_lswap;
	sw.sm_pageno = swappg;

	/* set in use bits */
	ASSERT(bftstclr(st->st_bmap, bitnum, fsize) == fsize);
	bfset(st->st_bmap, bitnum, (bitlen_t)fsize);
	st->st_gen++;
	
	swapunlock(s);
	*npgs = fsize;
	return((sm_swaphandle_t)sw.sm_all);
}

/*
 * sm_dealloc - free list of swap pages
 */
void
sm_dealloc(sm_swaphandle_t sh, int npgs)
{
	register pgno_t	pgnbr;
	register swapinfo_t *st;
	swaphandle_t sw;
	int s;

	s = swaplock();

	sw.sm_all = sh;
	ASSERT(sw.sm_lswap < MAXLSWAP);
	st = lswaptab[sw.sm_lswap];
	pgnbr = sw.sm_pageno - st->st_swppglo;
	ASSERT(bftstset(st->st_bmap, (bitnum_t)pgnbr, npgs));
	bfclr(st->st_bmap, (bitnum_t)pgnbr, npgs);
	st->st_nfpgs += npgs;
	ASSERT(st->st_nfpgs <= st->st_npgs);

	swapunlock(s);
}

/*
 * sm_getlswap - return logical swap # (for error messages mostly)
 */
short
sm_getlswap(sm_swaphandle_t sh)
{
	swaphandle_t sw;
	sw.sm_all = sh;
	return(sw.sm_lswap);
}

/*
 * sm_free - free list of swap pages
 */
void
sm_free(sm_swaphandle_t *sh, int npgs)
{
	register pgno_t	pgnbr;
	register swapinfo_t	*st;
	register int i;
	int s;
	swaphandle_t sw;

	s = swaplock();

	for (i = 0; i < npgs; i++, sh++) {
		if (!*sh) {
#ifdef DEBUG
			*sh = -1;
#endif
			continue;
		}
		sw.sm_all = *sh;
		ASSERT(sw.sm_lswap < MAXLSWAP);
		st = lswaptab[sw.sm_lswap];
		pgnbr = sw.sm_pageno - st->st_swppglo;
		ASSERT(btst(st->st_bmap, (bitnum_t)pgnbr));
		bclr(st->st_bmap, (bitnum_t)pgnbr);
		st->st_nfpgs++;
		ASSERT(st->st_nfpgs <= st->st_npgs);
#ifdef DEBUG
		*sh = -1;
#endif
	}
	swapunlock(s);
}

/*
 * sm_makeswaphandle - construct a swap handle given a dev/blkno pair
 * This is only used for async xfers to block devices..
 * Note that this does NOT work for anything but block devices!
 */
sm_swaphandle_t
sm_makeswaphandle(dev_t dev, daddr_t blkno)
{
	register int i;
	register swapinfo_t *st;
	swaphandle_t sw;

	sw.sm_all = 0;
	for (i = 0; i < NSWAPPRI; i++) {
		for (st = swaptab[i].sw_list; st; st = st->st_list) {
			if (st->st_vp->v_type != VBLK)
				continue;
			if (st->st_vp->v_rdev == dev) {
				sw.sm_lswap = st->st_lswap;
				sw.sm_pageno = dtop(blkno);
				break;
			}
		}
	}
	return((sm_swaphandle_t)sw.sm_all);
}

/*
 * swap I/O
 *
 * XXX it would be really nice if we could, for READS, simply read
 * the page into the page cache like pageins do. this would simplify
 * the vfault logic and permit read-ahead - a win for nfs swapping
 * The only problem is VBLK swap devices since we would have to
 * alloc the page and insert it in the hash.
 */
int
sm_swap(register pglst_t *pglptr, sm_swaphandle_t sh, int npgs,
						int rw, void (*done)(buf_t *))
{
	register buf_t *bp;
	register swapinfo_t *st;
	register int i;
	register pfd_t *npfd, *pfd;
	register dev_t dev;
	swaphandle_t sw;
	struct vnode *vp;
	int error;
	int s;
	/* REFERENCED */
#ifdef SWAPDEBUG
	pglst_t *spg = pglptr;
#endif

	ASSERT(npgs > 0);
	ASSERT((rw & B_ASYNC) == 0 || done);
	sw.sm_all = sh;
	st = lswaptab[sw.sm_lswap];

	ASSERT(sw.sm_pageno >= st->st_swppglo);
	ASSERT(sw.sm_pageno < (st->st_npgs + st->st_swppglo));
	vp = st->st_vp;
	dev = vp->v_rdev;

	bp = getphysbuf(dev);
	ASSERT(bp->b_un.b_addr == NULL);
	ASSERT(bp->b_remain == 0);
	ASSERT(bp->b_resid == 0);
	bp->b_flags = B_PAGEIO | B_BUSY | B_PHYS | B_SWAP | rw;
	bp->b_blkno = ptod(sw.sm_pageno);
	bp->b_bcount = ctob(npgs);
	bp->b_bufsize = bp->b_bcount;

	/*
	 * getpages relies on the fact that pchain is
	 * non-null to keep from chaining
	 * pfds on the list twice therefore
	 * be sure NOT to null out last page
	 * Note that this means that one MUST use the buffer size NOT just
	 * chain through list
	 */
	pglptr += (npgs - 2);	/* points to next to last */
	pfd = npfd = (pglptr+1)->gp_pfd;	/* last page */
	ASSERT(pfd->pf_pchain == NULL || pfd->pf_pchain == (pfd_t *)0xaddL);
	for (i = npgs-1; i; i--, pglptr--) {
		pfd = pglptr->gp_pfd;
		ASSERT(pfd->pf_pchain == NULL || pfd->pf_pchain == (pfd_t *)0xaddL);
		pfd->pf_pchain = npfd;
		npfd = pfd;
	}
	bp->b_pages = pfd;

	/* accounting */
	if (rw & B_READ) {
		/* sar, osview, etc. convert from pages to disk blocks */
		SYSINFO.swapin++;
		SYSINFO.bswapin += npgs;
	} else {
		SYSINFO.swapout++;
		SYSINFO.bswapout += npgs;
	}

	if (vp->v_type == VBLK) {
		if (rw & B_ASYNC)
			bp->b_relse = done;
		error = swapseg(dev, bp, rw);
	} else {
		/* swap to file! */
		uio_t auio;
		uio_t *uio = &auio;
		iovec_t aiovec;

		aiovec.iov_base = bp_mapin(bp);
		uio->uio_iov = &aiovec;
		uio->uio_iovcnt = 1;
		uio->uio_resid = aiovec.iov_len = bp->b_bcount;
		uio->uio_offset = ctob(sw.sm_pageno);
		uio->uio_segflg = UIO_NOSPACE;	/* no copying!!! */
		uio->uio_limit = RLIM_INFINITY;
		uio->uio_fmode = 0;
		uio->uio_sigpipe = 0;
		uio->uio_pmp = NULL;
                uio->uio_pio = 0;
		uio->uio_readiolog = 0;
		uio->uio_writeiolog = 0;
                uio->uio_pbuf = 0;

		/*
		 * no async file xfers since have no way to tell when
		 * they're done
		 * Note that its important that the VOP_WRITE doesn't require
		 * any memory - that could wedge vhand
		 */
		rw &= ~B_ASYNC;

		if (rw & B_READ) {
			VOP_READ(vp, uio, IO_DIRECT|IO_IGNCACHE|IO_SYNC|IO_PFUSE_SAFE, 
					sys_cred, sys_flid, error);
		}
		else {
			VOP_WRITE(vp, uio, IO_DIRECT|IO_IGNCACHE|IO_SYNC|IO_PFUSE_SAFE, 
					sys_cred, sys_flid, error);
		}
		ASSERT(uio->uio_sigpipe == 0);

#ifdef DEBUG
		if (!(rw & B_READ) && swapwrinject > 0 && --swapwrinject == 0) {
			swapwrinject = swapwrinjectfreq;
			error = EIO;
		}
		if ((rw & B_READ) && swaprdinject > 0 && --swaprdinject == 0) {
			swaprdinject = swaprdinjectfreq;
			error = EIO;
		}
#endif
		if (error) {
			s = swaplock();
			if (error == ESTALE)
				st->st_flags |= ST_STALE;
			else if (error == EACCES)
				st->st_flags |= ST_EACCES;
			else
				st->st_flags |= ST_IOERR;
			swapunlock(s);
		}
		ASSERT(error || uio->uio_resid == 0);
	}
	if (error) {
		cmn_err_tag(132,CE_ALERT, "Swap %s failed on logical swap %d blkno 0x%x for process [ %s ]",
				(rw & B_READ) ? "in" : "out", sm_getlswap(sh),
				bp->b_blkno, get_current_name());
		putphysbuf(bp);
		return(error);
	}
#ifdef SWAPDEBUG
	swapsum(spg, sw, npgs, rw);
#endif

	if (rw & B_ASYNC)
		return 0;

	/*
	 * a sync xfer - if they provided a 'done' function - call it
	 * else just free up bp and return
	 */
	if (done)
		(*done)(bp);
	else
		putphysbuf(bp);
	return 0;
}

static int
swapseg(dev_t dev, struct buf *bp, int rw)
{
	struct bdevsw *my_bdevsw;
#ifdef DEBUG
	if (!(rw & B_READ) && swapwrinject > 0 && --swapwrinject == 0) {
		swapwrinject = swapwrinjectfreq;
		bp->b_flags |= B_ERROR;
		return EIO;
	}
	if ((rw & B_READ) && swaprdinject > 0 && --swaprdinject == 0) {
		swaprdinject = swaprdinjectfreq;
		bp->b_flags |= B_ERROR;
		return EIO;
	}
#endif
	my_bdevsw = get_bdevsw(dev);
	ASSERT(my_bdevsw != NULL);
	bdstrat(my_bdevsw, bp);
	if (bp->b_flags & B_ERROR)
		return(EIO);

	if (rw & B_ASYNC)
		return(0);

	/* wait for transaction */
	ADD_SYSWAIT(swap);

	psema(&bp->b_iodonesema, PRIBIO|TIMER_SHIFT(AS_PHYSIO_WAIT));

	SUB_SYSWAIT(swap);

	if (bp->b_flags & B_ERROR)
		return(EIO);
	return(0);
}

/*
 * int
 * swaptabsnap(swapinfo_t **si_snapp)
 *	Function call to a snapshot of the swap devices
 *
 *	The length (number of swap entries) is returned.
 *	The information will have been copied to a newly allocated
 *	kernel address space and the passed in pointer (*si_snapp)
 *	will point to the information. The caller is responsible
 *	for calling kmem_free().
 */
int					/* number of entries in snapshot */
swaptabsnap (swapinfo_t **si_snapp)
{
	int  i, nswapfiles_snap;
	swapinfo_t *si_snap, *st, *sip;
	
retry:
	nswapfiles_snap = nswapfiles;
	
	si_snap = kmem_alloc(nswapfiles_snap * sizeof(swapinfo_t), 
					   KM_SLEEP);
	
	swapmodlock();
	
	/*
	 * If more files were added while we blocked, free our
	 * old memory reservation and try again. We want to give
	 * as accurate a picture as possible.
	 */
	if (nswapfiles != nswapfiles_snap) {
		swapmodunlock();
		kmem_free(si_snap, 
			  sizeof(swapinfo_t) * nswapfiles_snap);
		goto retry;
	}
	
	/*
	 * Copy the entire table.
	 */
	sip = si_snap;
	for (i = 0; i < NSWAPPRI; i++) {
		for (st = swaptab[i].sw_list; st; st = st->st_list) {
			*sip = *st;
			sip++;
		}
	}
	
	swapmodunlock();
	
	*si_snapp = si_snap;
	return (nswapfiles_snap);
}

