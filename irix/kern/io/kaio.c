/**************************************************************************
 *                                                                        *
 *          Copyright (C) 1996,1997,1998 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Kernel asynchronous i/o. Works only for raw disk devices.
 * User must ensure the blocks being written are L2-cache-line-size
 * aligned, or an error will be returned. Entry is via new
 * syssgi() calls. Normal access is through aio_read()/aio_write() in libc,
 * when aio_usedba was previously set in the aio_sgi_init() call or by
 * setting the __SGI_USE_DBA environment variable.
 */

/*
 * NOTE on special use of B_PARTIAL and B_LEADER flags (IA_WAR)
 *
 * B_PARTIAL is used with the actual I/O DMA buffer. The interrupt routine,
 * kaio_done, calls page_counter_undo to force the IA WAR dma counters to be
 * decremented immediately and any copied-behind-the-back pages to be copied
 * back. Later, kaio_cleanup runs the next time the user makes a KAIO call,
 * and at this time fast_undma() is called on the buffer with B_PARTIAL set
 * to indicate that the counters have already been decremented.
 *
 * B_LEADER is used with the user AIOCBs, which we want to pin in memory but
 * do not intend to do DMA into or out of. Since the blocks are not cache-line
 * aligned and also not cache-line sized, the IA WAR code would do unfortunate
 * things like copy the blocks around on useracc()/maputokv() and then copy them
 * back at unuseracc()/unmaputokv() time. This makes it impossible for the user
 * to see the done bit in the block early enough.
 * The IA WAR dma counting code in fast_useracc and fast_unuseracc will be
 * skipped when B_LEADER is set. This keeps an entry from being created, so
 * unmaputokv() will also avoid doing IA_WAR stuff when it runs.
 */

#ident    "$Header: /proj/irix6.5.7m/isms/irix/kern/io/RCS/kaio.c,v 1.49 1999/02/18 20:14:59 wombat Exp $"

#include "sys/errno.h"

#if defined(IP19) || defined(IP25) || defined(IP27) || defined (IP33)  /* Supported on big servers only */
#include "sys/types.h"
#include "sys/atomic_ops.h"
#include "ksys/vfile.h"
#include "ksys/fdt.h"
#include "sys/vnode.h"
#include "sys/kmem.h"
#include "sys/debug.h"
#include "sys/kabi.h"
#include "ksys/vproc.h"
#include "sys/proc.h"
#include "sys/pfdat.h"
#include "sys/immu.h"	/* for PG_* */
#include "sys/buf.h"	/* for B_READ */
#include "sys/poll.h"	/* for pollhead, etc. */
#include "sys/kaio.h"	/* for lots of stuff */
#include "sys/sysinfo.h"	/* for syswait */
#include "sys/lio.h"
#include "sys/resource.h"
#include "sys/sysmacros.h"
#include "sys/uthread.h"
#include "sys/sat.h"	/* for _SAT_FD_RDWR */
#include "sys/dbacc.h"
#include "sys/var.h"
#include "ksys/as.h"	/* for vaddr_to_pde */
#include "sys/nodepda.h" /* for SUB_SYSWAIT */
#include "sys/graph.h"
#include "sys/iograph.h" /* for EDGE_LBL_KAIO */

int kaiodevflag = D_MP /* sys/conf.h */;	/* master.d */
int kaio_lastdone;	/* Want this per-process */
int kaio_lastb = 0;

#if defined(DEBUG_KAIO) || defined(_SGI_WOMBAT)
/* For debugging */
kaiocb_t * debugme_kaiocb;
int kaio_printme = 0;

char kaiodbglog[12000];
int kaiodbgix = 0;
#endif

/*
 * Bits we should turn on to make the virtual address kernel addressable
 */
#define	GOOD	(PG_G | PG_M | PG_SV | PG_VR)
#pragma inline uaio_size

extern int max_sys_aio;	/* systunable, in master.d/kaio */


typedef struct aiofree {
	kaiocb_t *afr_head;
	kaiocb_t *afr_tail;
	lock_t	afr_lock;
	int 	afr_cnt;
} aiofree_t;
aiofree_t aio_free_list;

int kaioinit_done;
extern	int	enable_kaio_fast_path;

/* WARNING: There are hard-coded numbers here that depend on aiocb_t from aio.h */

/* size of the user space aiocb_t */
/* This is used to decide how much of the user aiocb should be pinned &
   mapped into kernel address space. Since we do not need the whole uaiocb,
   just make sure this is big enough for the fields we do need access to.
   Note the o32-abi aiocb64_t, which uses a field at the very end of the
   struct, so we do need to pin the whole thing for o32. */
int uaio_size(int abi) {
	if (ABI_IS_IRIX5_64(abi)) {
		return 320;
	} else if (ABI_IS_IRIX5(abi)) {
		/* do not forget about the offset64 field at the very end */
		return sizeof (struct useraio_abi32);
	} else /* n32 */ {
		return 168;
	}
}

kaiocb_t *kaiotbl;
void kaioattach(void);
/* Called from main() */
void
kaio_init()
{
	int inx;
	kaiocb_t *ap, *kaio;
	extern int dba_use_lgpg;

	if (kaioinit_done)
		return;
	kaioinit_done++;

	/* Create /hw/kaio */
	kaioattach();
	dba_use_lgpg = 1;
	kaiotbl = kaio = (kaiocb_t *)kmem_alloc(max_sys_aio * sizeof(kaiocb_t), KM_SLEEP | VM_CACHEALIGN);

	init_spinlock(&(aio_free_list.afr_lock), "aiolock", 1);
	aio_free_list.afr_head = kaio;
	aio_free_list.afr_tail = &kaio[max_sys_aio - 1];
	aio_free_list.afr_cnt = max_sys_aio;
	inx = 1;
	for (ap = aio_free_list.afr_head;
	     ap < aio_free_list.afr_tail;
	     ap = ap->aio_next_free) {
		ap->aio_next_free = &kaio[inx++];
		ap->aio_tasks = 0;
	}
	kaio[max_sys_aio - 1].aio_next_free = (kaiocb_t *)0;
}

int
kaio_installed(void)
{
	return SGI_DBACF_AIO | SGI_DBACF_AIO_PARAMS | SGI_DBACF_AIOSUSPEND;
}

void
kaio_getparams(struct dba_conf *dbc)
{
	dbc->dbcf_aio_max = max_sys_aio;
}

/*ARGSUSED*/
void
kaio_setparams(struct dba_conf *dbc)
{
	return;
}

struct kaiocbhd *
kaio_get_khd(int *error) {
	struct kaiocbhd *khd;
	*error = 0;

	CURVPROC_GETKAIOCBHD(khd);
	if (khd == NULL) {
		struct kaiocbhd *tkhd;
		int j;

		khd = (struct kaiocbhd *)kmem_alloc(sizeof(*khd), KM_SLEEP | VM_CACHEALIGN);    
		/* SEMA */
		/*printf("BETA 2 DBA 3.0: did kaio_init for pid %d\n", current_pid());*/
		init_sema(&(khd->aio_waitsema), 0, "aioex", 1); /* exit */
		init_spinlock(&(khd->susplock), "aiosp", 1); /* suspend bits */
		init_spinlock(&(khd->aiohd_lock), "aioq", 1); /* free list */
		initpollhead(&(khd->ph));
		for (j = 0; j < KAIO_NUMQ; j++) {
			khd->aio_hdfree[j] = 0;
		}
		khd->num_aio = 0;
		khd->num_aio_inprogress = 0;
		khd->proc_stat = 0;
		/* kaio_user_init will fill these in if it's the caller */
		khd->maxnaio = 0;
		khd->khpid=current_pid();	/*WWW*/
		for (j = 0; j < KAIO_MAXUTABLES; j++) {
			khd->susp[j].kaddr = 0;
			khd->susp[j].uaddr = 0;
			khd->susp[j].tasks = 0;
			khd->susp[j].cookie = 0;
		}

		CURVPROC_SETKAIOCBHD(khd, tkhd);
		if (tkhd != khd) {
			/* Some other uthread came in and did the
			 * same thing - tear down our whd that we have
			 * built.
			 */
			freesema(&khd->aio_waitsema);
			spinlock_destroy(&khd->aiohd_lock);
			spinlock_destroy(&khd->susplock);
			kmem_free(khd, sizeof(*khd));
			khd = tkhd;
		} else {
			/* We are the first ones to set up - add
			 * an exit callback
			 */
			*error = add_exit_callback(current_pid(), 0,
						  (void (*)(void *)) kaio_exit, 0);
			ASSERT(*error == 0);
		}
	}
	return khd;
}

/*
 * ========================================
 * Device driver entry points
 * ========================================
 */

/* This "driver" uses kaio_user_init() for most open-like initialization,
 * because syssgi(KAIO_USER_INIT) can pass more interesting args than the
 * driver open function could.
 *
 * /dev/kaio is used to hook into the poll/select syscall. Disk data
 * read/write is done through the syssgi(KAIO_READ/_WRITE) calls.
 * The kaio device provides a fake file descriptor that kaiopoll will
 * use to map to aiocbs.
 * When upper-level poll calls down to ask about the kaio fd, kaiopoll will check
 * aiocbs for all interesting fds.
 * This is inherently a per-process device, since you can't look at another
 * process's aiocbs.
 * User (libc) is assumed to have called syssgi(KAIO_USER_INIT) before open().
 */
void
kaioattach(void)
{
     dev_t dev;
     graph_error_t rv;
     vertex_hdl_t vert;

     rv = hwgraph_char_device_add(GRAPH_VERTEX_NONE, EDGE_LBL_KAIO, "kaio", &dev);
     ASSERT(rv == GRAPH_SUCCESS);
     if (rv != GRAPH_SUCCESS) {
	  cmn_err(CE_WARN, "kaio driver not attached to hwgraph\n");
     }
     if ((vert = hwgraph_path_to_vertex(EDGE_LBL_KAIO)) == GRAPH_VERTEX_NONE) {
	  cmn_err(CE_WARN, "kaio drive unable to find /hw/kaio to change modes\n");
     }
     hwgraph_chmod(vert, 0666);
}



/*ARGSUSED*/
kaioopen(dev_t *devp, int flags, int otyp, struct cred *crp)
{
	struct kaiocbhd *khd;
	int err = 0;

	khd = kaio_get_khd(&err);
	if (err)
	     return err;
	if (khd == NULL) {
	     return EAGAIN;
	}

	/* Must follow protocol to avoid accidents - kaio_user_init, then open */
	if (khd->susp[KAIO_COMPL].kaddr && khd->susp[KAIO_WAIT].kaddr)
		return 0;
	else
		return EINVALSTATE;
}

/*ARGSUSED*/
int kaioclose(dev_t dev, int flag, int otyp, struct cred *crp)
{
     /* Reclaim PRDA */
     return 0;
}

/* What if user manages to nest calls? -> their problem */
/*ARGSUSED*/
int kaiopoll(dev_t dev, register short events, int anyyet, short *reventsp,
	 struct pollhead **phpp, unsigned int *genp)
{
	/* Check outstanding aiocbs to see if any are ready. */
	struct kaiocbhd *khd;
	short happened = 0;
	unsigned int gen;
	int err = 0, matchdone = 0, matchwait = 0;

	khd = kaio_get_khd(&err);
	if (err)
	     return err;
	if (khd == 0)
	     return EAGAIN;
	gen = POLLGEN(&khd->ph);
#ifdef DEBUG_KAIO
if(kaio_printme > 1){
printf("kaiopoll: khd = 0x%x, gen = %d, lastdone = %d\n", khd, gen, kaio_lastdone);
printf("kaiopoll: compl @ 0x%x, wait @ 0x%x, ph = 0x%x @ 0x%x\n",
&(khd->susp[KAIO_COMPL].kaddr[0]),&(khd->susp[KAIO_WAIT].kaddr[0]),khd->ph,&(khd->ph));
}
#endif
	if (events & (POLLIN | POLLRDNORM)) {
		if (kaio_lastdone && (kaio_lastdone < khd->maxnaio)
		    && (khd->susp[KAIO_COMPL].kaddr[kaio_lastdone] == KAIOSEL_DONE)
		    && (khd->susp[KAIO_WAIT].kaddr[kaio_lastdone] == KAIOSEL_WAITING)) {
			happened |= POLLIN | POLLRDNORM;
#ifdef DEBUG_KAIO
if(kaio_printme > 1)
printf("kaiopoll: found a match on lastdone\n");
#endif
		} else {
			char c, w;
			int j;
			/* Must search through all looking for one */
			for (j = 1; j < khd->maxnaio; j++) {
			     c = khd->susp[KAIO_COMPL].kaddr[j];
			     w = khd->susp[KAIO_WAIT].kaddr[j];
			     if ((c == KAIOSEL_DONE) && (w == KAIOSEL_WAITING)) {
				     happened |= POLLIN | POLLRDNORM;
#ifdef DEBUG_KAIO
if(kaio_printme > 1)
printf("kaiopoll: found a match ix = %d\n", j);
#endif
			     }
			     if (c == KAIOSEL_DONE)
				     matchdone++;
			     if (w == KAIOSEL_WAITING)
				     matchwait++;
		     }
		}
	}

	*reventsp = happened;

	if (!happened && !anyyet) {
		*phpp = &khd->ph;
		*genp = gen;
#ifdef DEBUG_KAIO
if(kaio_printme > 1)
printf("kaiopoll: no match, new gen = %d\n", gen);
#endif
	}
#ifdef DEBUG_KAIO
if(kaio_printme > 1)
printf("kaiopoll returning: matchdone = %d, matchwait = %d, happened = %d, anyyet = %d, lastdone = %d\n", matchdone, matchwait, happened, anyyet,kaio_lastdone);
#endif
	return 0;
}

/* ARGSUSED */
int
kaioioctl(dev_t dev, int cmd, __psint_t arg, int mode, struct cred *crp, int *rvalp) {
	return EINVAL;
}

/*
 * ========================================
 * End device driver entry points
 * ========================================
 */


int
kaio_pin_me(void *uaddr, int len, int *cookiep, char **kaddr, int *tasks)
{
	int error, dounmaputokv = 0;
#if 0
	useraio_t utemp;
#endif
	/* Lots of things are shared between user space and kernel. Mostly
	   it's aiocbs but if using aio_suspend, also some waiting/done tables. */
	/* Note cookiep will only ever have one element at a time for kaio, at
	 * least until kaio_listio exists. */
        error = fast_useracc(uaddr, len,
#ifdef EVEREST
			     ((io4ia_war && io4ia_userdma_war) ? B_LEADER : 0) |
#endif
			     B_READ | B_PHYS,
			     cookiep);
	if (error) {
		/* Must return error now if useracc failed, since maputokv
		   always succeeds, even if user address is invalid. */
		if ((error != EFAULT) && (error != EAGAIN)) {
			printf("kaio_pin_me: Got error %d from fast_useracc\n",
			       error);
		}
		goto out; /* WWW ???? */
	}
	if (enable_kaio_fast_path
	    && (pnum(uaddr) == pnum((caddr_t)uaddr + len))) {
		pde_t user_pde;
		pfn_t pfn;
		/* If all contained on one page, use K0, known to be valid
		 * because fast_useracc was just called */
		vaddr_to_pde((caddr_t)uaddr, &user_pde);
if (pg_getpgi(&user_pde) == 0){
cmn_err(CE_WARN, "Fast path but not on pinned page? uaddr = 0x%llx\n", (long long)uaddr);
}
		pfn = pg_getpfn(&user_pde);
		*kaddr = (char *)(small_pfntova_K0(pfn) 
				  + poff(uaddr));
		*tasks |= KAIO_DO_NOT_UNALLOC;
	} else {
		*kaddr = (char *)maputokv((caddr_t)uaddr, len,
					  pte_cachebits() | GOOD);
		dounmaputokv = KAIO_UNMAPUTOKV;
	}
	if (*kaddr == NULL)  /* Why no EAGAIN here? */
		goto out;
	*tasks |= KAIO_UNPIN|dounmaputokv;

#if 0
/***DEBUG***/
/*WWW - do not release with these checks/panics */
if (error = copyin((void *)uaddr, (void *)&utemp, sizeof(utemp))) {
panic("kaio_pin_me: Sanity check could not copyin user-space aiocb from 0x%x to 0x%x\n", uaddr, &utemp);
}
if (bcmp(*kaddr, &utemp, sizeof(utemp))) {
panic("kaio_pin_me: pinned area does not match user 0x%x/0x%x\n",*kaddr,&utemp);
}
/***END_DEBUG***/
#endif
	return 0;

 out:
#ifdef DEBUG
printf("kaio_pin_me: something bad happened, uaddr = 0x%x, len = %d, kaddr = 0x%x, error = %d\n", uaddr,len,*kaddr,error);
#endif
	if (uaddr) {
		if (*tasks & KAIO_UNPIN) {
			fast_unuseracc((void *)uaddr, len,
#ifdef EVEREST
				       ((io4ia_war && io4ia_userdma_war) ? B_LEADER : 0) |
#endif
				       B_READ | B_PHYS,
				       cookiep);
			*kaddr = 0;
			*tasks &= ~KAIO_UNPIN;
		}
	}
	return error;
}

/* Called from syssgi(1203,..) */
/*ARGSUSED*/
int
kaio_user_init(void *vaddr, long count)
{
	struct kaioinit	kuinit;
	struct kaiocbhd *kh;
	int error = 0, j, rv;

	if (copyin((void *)vaddr, (void *)&kuinit, MIN(count, sizeof kuinit))) {
		kuinit.k_err = EFAULT;
		return -1;
	}
	if (kuinit.k_version != KAIOUSER_1_1) {
	     cmn_err(CE_WARN, "KAIO kernel/library mismatch, library = %d, kernel = %d\n", kuinit.k_version, KAIOUSER_1_1);
		kuinit.k_err = EINVAL;
		return -1;
	}

	kh = kaio_get_khd(&error);
	if ((kh == NULL) || error) {
		/* Something has gone horribly wrong */
		cmn_err(CE_WARN, "No kaiocbhd, error = %d!\n", error);/*WWW*/
		if (error)
			kuinit.k_err = error;
		else
			kuinit.k_err = -1;
		error = -1;
		return -1;
	}

	/* As for kuaiocb structs, susp[*].uaddr is in user space and kaio_done
	   will want to r/w there at interrupt time (no DMA). Need to pin them
	   and give them kernel addresses. */
	kh->maxnaio = MIN(kuinit.k_naio, max_sys_aio);
	kuinit.k_naio_used = kh->maxnaio;
	kh->susp[KAIO_COMPL].uaddr = (char *)kuinit.k_completed;
	kh->susp[KAIO_WAIT].uaddr = (char *)kuinit.k_waiting;
	for (j = 0; j < KAIO_MAXUTABLES; j++ ) {
		if (rv = kaio_pin_me((void *)kh->susp[j].uaddr, kh->maxnaio,
				     &(kh->susp[j].cookie),
				     &kh->susp[j].kaddr, &kh->susp[j].tasks)) {
			kuinit.k_err = rv;
			goto out;
		}
		if (kh->susp[j].kaddr == NULL) {
			kuinit.k_err = EAGAIN;
			goto out;
		}
		if (kh->susp[j].tasks & KAIO_UNPIN)
			kh->susp[j].tasks |= KAIO_UNLOCKUTBL;
	}

	if (copyout((void *)&kuinit, (void *)vaddr, MIN(sizeof kuinit, count))) {
		/* Too late to fill in k_err */
		return -1;
	}
	return 0;

 out:
	for (j = 0; j < KAIO_MAXUTABLES; j++ ) {
		/* Look at UNPIN because UNLOCKUTBL not set yet */
		if (kh->susp[j].tasks & KAIO_UNPIN) {
			fast_unuseracc((void *)kh->susp[j].uaddr,
				       kh->maxnaio,
#ifdef EVEREST
				       ((io4ia_war && io4ia_userdma_war) ? B_LEADER : 0) |
#endif
				       B_READ | B_PHYS,
				       &(kh->susp[j].cookie));
		}
		if ((kh->susp[j].tasks & KAIO_UNMAPUTOKV)
		    && !(kh->susp[j].tasks & KAIO_DO_NOT_UNALLOC)) {
			unmaputokv(kh->susp[j].kaddr, kh->maxnaio);
		}
		kh->susp[j].uaddr = 0;
		kh->susp[j].kaddr = 0;
		kh->susp[j].tasks = 0;
	}
	kh->maxnaio = 0;

	return error;
}

/* Do all the things requiring a user context or semawait that
 * are normally done when an i/o op completes.
 */
/* called by kaio_get_free(), kaio_get_free_new(), kaio_exit() */
void
kaio_cleanup(struct kaiocb *wp) 
{
	if (wp->aio_tasks) {
		int abi = get_current_abi();
		int uaiosz = uaio_size(abi);
		if (wp->aio_tasks & KAIO_UNDMA) {
			/*
			 * Use B_PARTIAL to indicate that io4ia counters have
			 * already been cleared (by kaio_done).
			 */
			/* Must have process context for this */
			fast_undma(wp->aio_iov.iov_base, wp->aio_iov.iov_len,
				   B_PARTIAL | (wp->aio_ioflag & B_READ), &wp->aio_undma_cookie);
		}

		/* unlock uaiocb */
		if (wp->aio_tasks & KAIO_UNLOCKAIOCB) {
			/* Use B_LEADER to indicate page was only locked, no DMA */
			fast_unuseracc((void *)wp->aio_uaiocb,
                                       uaiosz,
#ifdef EVEREST
				       ((io4ia_war && io4ia_userdma_war) ? B_LEADER : 0) |
#endif
                                       B_READ | B_PHYS,
                                       &wp->aio_uiocb_cookie);
		}

		if ((wp->aio_tasks & KAIO_UNMAPUTOKV) && !(wp->aio_tasks & KAIO_DO_NOT_UNALLOC)) {
			/* No user context required - must be done after fast_unuseracc */
			unmaputokv((caddr_t)wp->aio_kuaiocb, uaiosz);
		}
	}
	wp->aio_tasks = 0;
#ifndef DEBUG
	wp->aio_proc = 0;
#endif
}

/* called by kaio_rw() */
int
kaio_get_free(kaiocb_t **wpp)
{
	kaiocb_t *wp;
	struct kaiocbhd *whd;
	int s1, s;
	/* REFERENCED */
	int error;

	whd = kaio_get_khd(&error);
	if (error)
		return error;
	if (!whd)
		return EAGAIN;

	ASSERT(wpp);

	s1 = io_splock(whd->aiohd_lock);

	whd->num_aio_inprogress++;
	DBA_STATS->kaio_inprogress++;

	/* Possible future optimization - if not called by exit, don't
	 * bother to unpin/repin uaiocb if addresses match
	 */

	/*
	 * Run all blocks on local KAIO_QCLEANUP through kaio_cleanup(),
	 * pushing onto KAIO_QFREE as finished. When done, pop first free
	 * block off, if any.
	 */
	wp = whd->aio_hdfree[KAIO_QCLEANUP];
	while (wp) {
		whd->aio_hdfree[KAIO_QCLEANUP] = wp->aio_next_free;
		io_spunlock(whd->aiohd_lock, s1);
		ASSERT(wp->aio_stat == AIOCB_DONE || wp->aio_stat == AIOCB_FREE);
		if (wp->aio_stat == AIOCB_DONE) {
			kaio_cleanup(wp);
			wp->aio_stat = AIOCB_FREE;
		}
		wp->aio_hd = whd;
		s1 = io_splock(whd->aiohd_lock);
		wp->aio_next_free = whd->aio_hdfree[KAIO_QFREE];
		whd->aio_hdfree[KAIO_QFREE] = wp;
		wp = whd->aio_hdfree[KAIO_QCLEANUP];
	}
	*wpp = wp = whd->aio_hdfree[KAIO_QFREE];
	if (wp) {
		whd->aio_hdfree[KAIO_QFREE] = wp->aio_next_free;
		wp->aio_next_free = (kaiocb_t *)0;
		io_spunlock(whd->aiohd_lock, s1);
		return 0;
	}

	/* No local block available - go to global list */
	s = io_splock(aio_free_list.afr_lock);
	*wpp = aio_free_list.afr_head;
	wp = *wpp;
	if (wp) {
		aio_free_list.afr_head = wp->aio_next_free;
		if (aio_free_list.afr_tail == wp) {
			aio_free_list.afr_tail = (kaiocb_t *)0;
		}

		wp->aio_next_free = (kaiocb_t *)0;
		aio_free_list.afr_cnt--;
		ASSERT(aio_free_list.afr_cnt >= 0);
		io_spunlock(aio_free_list.afr_lock, s);
		wp->aio_stat = AIOCB_FREE;
		wp->aio_hd = whd;
		whd->num_aio++;
		if (DBA_STATS->kaio_proc_maxinuse < whd->num_aio)
			DBA_STATS->kaio_proc_maxinuse = whd->num_aio;
		io_spunlock(whd->aiohd_lock, s1);
		DBA_STATS->kaio_aio_inuse++;
		return 0;
	} else {
		whd->num_aio_inprogress--;
		DBA_STATS->kaio_nobuf++;
		DBA_STATS->kaio_inprogress--;
		io_spunlock(aio_free_list.afr_lock, s);
		io_spunlock(whd->aiohd_lock, s1);
		return EAGAIN;
	}
	/*NOTREACHED*/
}

/* called by kaio_rw(), kaio_rw_new()-NEVER, kaio_done() */
void
kaio_put_free(kaiocb_t *wp, int whichq)
{
	struct kaiocbhd *whd;
	int s;
	ASSERT(wp);

	wp->aio_next_free = (kaiocb_t *)0;

	whd = wp->aio_hd;
	DBA_STATS->kaio_inprogress--;
	s = io_splock(whd->aiohd_lock);
	whd->num_aio_inprogress--;
	wp->aio_next_free = whd->aio_hdfree[whichq];
	whd->aio_hdfree[whichq] = wp;
	if (whd->proc_stat == AIOHD_WAITIO)
		vsema(&(whd->aio_waitsema));
	ASSERT(whd->num_aio_inprogress >= 0);
	io_spunlock(whd->aiohd_lock, s);
}

/* called by kaio_exit() */
void
kaio_free(kaiocb_t *wp)
{
	int s;

	ASSERT(wp);
	ASSERT(wp->aio_tasks == 0);

	wp->aio_stat = AIOCB_FREE;
	wp->aio_next_free = (kaiocb_t *)0;
	s = io_splock(aio_free_list.afr_lock);

	if (aio_free_list.afr_head) {
		ASSERT(aio_free_list.afr_tail);
		wp->aio_next_free = aio_free_list.afr_head;
		aio_free_list.afr_head = wp;
	} else {
		ASSERT(aio_free_list.afr_tail == 0);
		aio_free_list.afr_head = wp;
		aio_free_list.afr_tail = wp;
	}
	aio_free_list.afr_cnt++;
	io_spunlock(aio_free_list.afr_lock, s);
	DBA_STATS->kaio_aio_inuse--;
}


kaiocb_t *most_recent_kaiocb;

/* called by syssgi() */
int
kaio_rw(useraio_t *uaiocb, int rw)
{
	struct vfile *fp;
	opaque_t uiocb_cookie=0; /* This is copied into kaiocb for kaio_done() */
	useraio_t *kuaiocb;
	int error = 0;
	int ioflag = 0;
	kaiocb_t *kaiocb = 0;
	uthread_t *ut;
	vnode_t	*vp;
	caddr_t buf;
	int len, fd, s;
	off_t offset;
	struct kaiocbhd *khd;
	int abi = get_current_abi();
	int uaiosz = uaio_size(abi);

	/* Get a free state-holder -- this also forces cleanup to run */
	if ((error = kaio_get_free(&kaiocb))) {
		goto out;
	}
most_recent_kaiocb = kaiocb;
/* WWW use kaio_pin_me, if possible */
#define KAIO_NO_PINME
#ifdef KAIO_NO_PINME
	/* must do useracc before maputokv */
	/* kernel will be reading/writing here in iodone function (kaio_done) */
        error = fast_useracc((void *)uaiocb,
			     uaiosz,
#ifdef EVEREST
			     ((io4ia_war && io4ia_userdma_war) ? B_LEADER : 0) |
#endif
			     B_READ | B_PHYS,
			     &uiocb_cookie);
	if (error) {
		/* Must return error now if useracc failed, since maputokv
		   always succeeds, even if user address is invalid. */
	     goto out;
	}

	kaiocb->aio_uaiocb = uaiocb;	/* so it can be found later in case of errors */
	if (enable_kaio_fast_path
	    && (pnum(uaiocb) == pnum((caddr_t)uaiocb + sizeof(useraio_t)))) {
		pde_t user_pde;
		pfn_t pfn;
		/* If all contained on one page, use K0 */
		vaddr_to_pde((caddr_t)uaiocb, &user_pde);
		pfn = pg_getpfn(&user_pde);
		kuaiocb = (useraio_t *)(small_pfntova_K0(pfn) 
					+ poff(uaiocb));
		kaiocb->aio_tasks |= KAIO_DO_NOT_UNALLOC;
	} else {
		kuaiocb = (useraio_t *)maputokv((caddr_t)uaiocb, uaiosz,
						pte_cachebits() | GOOD);
	}
	if (kuaiocb == NULL) {
		error = EAGAIN;
		goto out;
	}
#else
	error = kaio_pin_me(uaiocb, uaiosz, &uiocb_cookie, (char **)&kuaiocb, &kaiocb->aio_tasks);
	kaiocb->aio_uaiocb = uaiocb;	/* so it can be found later in case of errors */
	if (error)
		goto out2;
#endif
	SYNCHRONIZE();
	GET_KUAIO_BUF(kuaiocb, abi, buf);
	GET_KUAIO_FILDES(kuaiocb, abi, fd);
	GET_KUAIO_NBYTES(kuaiocb, abi, len);
	if (len == 0) {
		/* complete! return error */
		SET_KUAIO_RET(kuaiocb, abi, 0, 0);
		SET_KUAIO_NOBYTES(kuaiocb, abi, 0, 0);
		/* SYNC not needed here, since user is here until the return */
		SET_KUAIO_ERRNO(kuaiocb, abi, 0);
		goto out2;
	}

	if (len < 0) {
		error = EINVAL;
		goto out2;
	}
	if (error = getf(fd, &fp)) {
		goto out2;
	}
	if (rw) {
		if ((fp->vf_flag & FWRITE) == 0) {
			_SAT_FD_RDWR(fd, FWRITE, EBADF);
			error = EBADF;
			goto out2;
		}
	} else {
		if ((fp->vf_flag & FREAD) == 0) {
			_SAT_FD_RDWR(fd, FREAD, EBADF);
			error = EBADF;
			goto out2;
		}
	}

	if (!VF_IS_VNODE(fp)) {
		error = EINVAL;
		goto out2;
	}
	vp = VF_TO_VNODE(fp);
	if (vp->v_type != VCHR) {
		error = EINVAL;
		goto out2;
	}

	ASSERT(kaiocb->aio_stat == AIOCB_FREE);
	kaiocb->aio_stat = AIOCB_INPROGRESS;

	/* want to write here at interrupt level -> no copyout. */
	kaiocb->aio_kuaiocb = kuaiocb;

	ut = curuthread;

	/* fill kaiocb */
	kaiocb->aio_tasks |= (KAIO_UNLOCKAIOCB | KAIO_UNMAPUTOKV);
	kaiocb->aio_fp = fp;
	kaiocb->aio_proc = current_pid(); /* for signal delivery (LATER - not supported yet) */
	kaiocb->aio_p_abi = get_current_abi();	/* Abi of requesting proc */

	kaiocb->aio_uio.uio_pio = KAIO;	/* for raw IO handling */
	kaiocb->aio_uio.uio_pbuf = (struct buf *)kaiocb;  /* have to store it somewhere */

	kaiocb->aio_iov.iov_base = (caddr_t)buf;
	kaiocb->aio_uio.uio_resid = kaiocb->aio_iov.iov_len = len;
	kaiocb->aio_uio.uio_iov = &kaiocb->aio_iov;
	kaiocb->aio_uio.uio_iovcnt = 1;
	kaiocb->aio_uaiocb = uaiocb; /* to unlock later in kaio_cleanup*/
	kaiocb->aio_uiocb_cookie = uiocb_cookie;
	/* For aio_suspend */
	CURVPROC_GETKAIOCBHD(khd);
	if (khd) {
		/* Pick a starting slot for search in wait/compl arrays */
		unsigned int tstslot = (((unsigned long)kuaiocb >> 5)&0x3ff);
		int up = (int)(((unsigned long)kuaiocb) & 0x10);

		kaiocb->aio_acompp = khd->susp[KAIO_COMPL].kaddr;
		kaiocb->aio_awaitp = khd->susp[KAIO_WAIT].kaddr;
		kaiocb->aio_maxnaio = khd->maxnaio;
		kaiocb->aio_ph = &(khd->ph);
		kaiocb->aio_susplock = &(khd->susplock);

		/* Look for a free byte position in akwaiting */
		/* Slot 0 is special and must not be allocated here */
		if (up) {
		     /* Search forward for a free slot */
		     if (tstslot == 0) { tstslot = 1; }
		     s = io_splock(*(kaiocb->aio_susplock));
		     while ((kaiocb->aio_awaitp[tstslot] != KAIOSEL_NOTINUSE)
			    && (tstslot < (kaiocb->aio_maxnaio - 1)))
			  tstslot++;
		     /* Did we find one yet? */
		     if (kaiocb->aio_awaitp[tstslot] != KAIOSEL_NOTINUSE) {
			  /* apparently not */
			       tstslot = 1;
			  while ((kaiocb->aio_awaitp[tstslot] != KAIOSEL_NOTINUSE)
				 && (tstslot < (kaiocb->aio_maxnaio - 1)))
				    tstslot++;
		     }
		} else {
		     /* Search backward for a free slot */
		     if (tstslot < 3) { tstslot = kaiocb->aio_maxnaio - 1; }
		     s = io_splock(*(kaiocb->aio_susplock));
		     while ((kaiocb->aio_awaitp[tstslot] != KAIOSEL_NOTINUSE)
				&& (tstslot > 1))
			       tstslot--;
		     /* Did we find one yet? */
		     if (kaiocb->aio_awaitp[tstslot] != KAIOSEL_NOTINUSE) {
			  /* apparently not */
			       tstslot = kaiocb->aio_maxnaio - 1;
			  while ((kaiocb->aio_awaitp[tstslot] != KAIOSEL_NOTINUSE)
				 && (tstslot > 1))
				    tstslot--;
		     }
		}
		if ((tstslot == 0) || (tstslot == kaiocb->aio_maxnaio)) {
		     /* Should never happen */
		     io_spunlock(*(kaiocb->aio_susplock), s);
		     DBA_STATS->kaio_io_errs++;
#ifdef DEBUG
		     printf("kaio_rw: tstslot %d, pid %d, %s, start from 0x%llx!\n",
			    tstslot, kaiocb->aio_proc, up?"up":"down",
			    (((unsigned long)kuaiocb >> 5)&0x3ff));
#endif
		     error = EAGAIN;
		     goto out2;
		}
		if (kaiocb->aio_awaitp[tstslot] != KAIOSEL_NOTINUSE) {
		     /* No free slots */
		     io_spunlock(*(kaiocb->aio_susplock), s);
#ifdef DEBUG
		     printf("kaio_rw: kaio/select aio index for pid %d is full - too many simultaneous requests!\n",
			    kaiocb->aio_proc);
#endif
		     error = EAGAIN;
		     goto out2;
		}

		/* Now we have a slot */
		SET_KUAIO_SELBYTE(kuaiocb, kaiocb->aio_p_abi, tstslot);
		kaiocb->aio_awaitp[tstslot] = KAIOSEL_INUSE;
		io_spunlock(*(kaiocb->aio_susplock), s);

		if ((tstslot > 0) && (tstslot < khd->maxnaio)) {
			kaiocb->aio_acompp[tstslot] = KAIOSEL_NOTDONE;
		}
	} else {
		kaiocb->aio_acompp = 0;
		kaiocb->aio_awaitp = 0;
		kaiocb->aio_maxnaio = 0;
		kaiocb->aio_ph = 0;
	}

	/* statistics */
	if (rw) {
		SYSINFO.syswrite++;
		ut->ut_acct.ua_syscw++;
		DBA_STATS->kaio_writes++;
	} else {
		SYSINFO.sysread++;
		ut->ut_acct.ua_syscr++;
		DBA_STATS->kaio_reads++;
	}

	GET_KUAIO_OFFSET(kuaiocb, abi, kaiocb->aio_offset);
	kaiocb->aio_uio.uio_offset = kaiocb->aio_offset;
	kaiocb->aio_vp = vp;

	/* 
	 * Do lseek. Always SEEK_SET for POSIX aio - no adjustment required.
	 */
	VFILE_GETOFFSET(fp, &offset);
	VOP_SEEK(vp, offset, &kaiocb->aio_offset, error);
	if (error)
	     goto out2;

	/*
	 * Initiate IO
	 */
	kaiocb->aio_uio.uio_segflg = UIO_USERSPACE;
	kaiocb->aio_uio.uio_limit = getfsizelimit();
	kaiocb->aio_uio.uio_pmp = NULL;
	kaiocb->aio_uio.uio_sigpipe = 0;
	kaiocb->aio_uio.uio_readiolog = 0;
	kaiocb->aio_uio.uio_writeiolog = 0;

	/* Unlike klistio, no need to do setjmp since this is a non-blocking call */
	SET_KUAIO_NOBYTES(kuaiocb, abi, 0, 0);
	SET_KUAIO_RET(kuaiocb, abi, 0, 0);
	/* SYNC not needed here */
	SET_KUAIO_ERRNO(kuaiocb, abi, EINPROGRESS);

	if (rw == 0) {
		VOP_READ(vp, &kaiocb->aio_uio, ioflag, fp->vf_cred, NULL, error);
	}
	else {
		VOP_WRITE(vp, &kaiocb->aio_uio, ioflag, fp->vf_cred, NULL, error);
		ASSERT(kaiocb->aio_uio.uio_sigpipe == 0);
	}

	if (error == 0) {
		/* just have to assume IO completes OK because we don't have
		 * the user context to update stats when IO is really done
		 */   
		UPDATE_IOCH(ut, ((u_long)len));
		switch (rw) {
		case 0:
			ut->ut_acct.ua_bread += len;
			_SAT_FD_RDWR(fd, FREAD, 0);
			break;
		case 1:
			ut->ut_acct.ua_bwrit += len;
			_SAT_FD_RDWR(fd, FWRITE, 0);
			break;
		}
		return 0;
	}

 out2:
	if (!(kaiocb->aio_tasks & KAIO_DO_NOT_UNALLOC)) {
		unmaputokv((caddr_t)kuaiocb, uaiosz);
	}

 out:
#ifdef KAIO_NO_PINME
	if (kaiocb && kaiocb->aio_uaiocb) {
		fast_unuseracc((void *)kaiocb->aio_uaiocb,
			       uaiosz,
#ifdef EVEREST
			       ((io4ia_war && io4ia_userdma_war) ? B_LEADER : 0) |
#endif
			       B_READ | B_PHYS,
			       &kaiocb->aio_uiocb_cookie);
		kaiocb->aio_uaiocb = 0;
	}
#else
	if (kaiocb)
		kaiocb->aio_uaiocb = 0;
#endif
	if (kaiocb) {
		kaiocb->aio_tasks = 0;
		kaiocb->aio_stat = AIOCB_FREE;
		kaio_put_free(kaiocb, KAIO_QFREE);
	}
	return error;
	/* kaio */
}

/*ARGSUSED*/
int
kaio_suspend(/*some struct*/int *a, int b)
{
	/* See kaiopoll */
	return ENOSYS;
}

/* biophysio() sets up hook, called from driver instead of iodone */
void
kaio_done(struct buf *bp)
{
	kaiocb_t *kaiocb;
	useraio_t *kuaiocb;
	int rval32, bperr;
	long rval64, dn;

	/* TO BE DONE: check for process state? is it waiting to exit? */
	ASSERT(bp->b_flags & B_BUSY);
	ASSERT(!(bp->b_flags & B_DONE));

#ifdef EVEREST
	if (io4ia_war && io4ia_userdma_war) {
		page_counter_undo(bp->b_un.b_addr, bp->b_bcount,
				  ((bp->b_flags & B_READ) == 0));
	}
#endif

	/* Lock down user aiocb page(s), check pointer is valid */
	kaiocb = bp->b_fsprivate;
	ASSERT(kaiocb);
	ASSERT(kaiocb->aio_stat == AIOCB_INPROGRESS);
	kuaiocb = kaiocb->aio_kuaiocb;
	ASSERT(kaiocb->aio_kuaiocb);

	/* "return" values */
	rval32 = kaiocb->aio_iov.iov_len - bp->b_resid;
	rval64 = kaiocb->aio_iov.iov_len - bp->b_resid;
	bperr = geterror(bp);
	if (bperr == 0) {
		/*int abi = kaiocb->aio_p_abi;*/
		SET_KUAIO_RET(kuaiocb, kaiocb->aio_p_abi, rval32, rval64);
		SET_KUAIO_NOBYTES(kuaiocb, kaiocb->aio_p_abi, rval32, rval64);
	} else {
	     SET_KUAIO_RET(kuaiocb, kaiocb->aio_p_abi, (int)-1, (long)-1);
	     SET_KUAIO_NOBYTES(kuaiocb, kaiocb->aio_p_abi, (int)-1, (long)-1);
	     DBA_STATS->kaio_io_errs++;
	}
	SYNCHRONIZE();
	SET_KUAIO_ERRNO(kuaiocb, kaiocb->aio_p_abi, bperr);

	kaiocb->aio_stat = AIOCB_DONE;
	bp->b_flags |= B_DONE;
	if (bp->b_flags & B_READ) {
		SYSINFO.readch += (u_long)rval64;
		DBA_STATS->kaio_read_bytes += (u_long)rval64;
	} else {
		SYSINFO.writech += (u_long)rval64;
		DBA_STATS->kaio_write_bytes += (u_long)rval64;
	}

	if (!(bp->b_flags & B_MAPUSER)) {
		iounmap(bp);
	}

	if (kaiocb->aio_tasks & KAIO_PUTPHYSBUF) {
		bp->b_iodone = 0;
		bp->b_fsprivate = 0;	/* If not cleared XFS gets confused */
		if (BP_ISMAPPED(bp))
			bp_mapout(bp);
		freerbuf(bp);
		kaiocb->aio_tasks &= ~KAIO_PUTPHYSBUF;
	}

	if (kaiocb->aio_acompp) {
		GET_KUAIO_SELBYTE(kuaiocb, kaiocb->aio_p_abi, dn);
		if (dn && (dn < kaiocb->aio_maxnaio)
		    && (dn > 0)) {
			/* Mark it done and wake up any waiters */
			kaiocb->aio_acompp[dn] = (char)KAIOSEL_DONE;	/*done*/

			if (kaiocb->aio_ph
			    && (kaiocb->aio_awaitp[dn] & KAIOSEL_WAITING)) {
				/* Wakeup */
				kaio_lastdone = dn; /*cache this*/
				pollwakeup(kaiocb->aio_ph, POLLIN|POLLRDNORM);
			}
		} else {
			/* user has blown it - index is out of range */
		}
	}

	/* put back on free list. undo mappings, etc. when allocated next */
	kaio_put_free(kaiocb, KAIO_QCLEANUP);

	SUB_SYSWAIT(physio);
}

/* called by exit() */
void
kaio_exit()
{
	struct kaiocbhd *whd;
	kaiocb_t *wp, *owp;
	int s, j;

	/* Single threaded at this point */
	CURVPROC_GETKAIOCBHD(whd);
	if (whd != NULL) {
		s = io_splock(whd->aiohd_lock);
		while (whd->num_aio_inprogress > 0) {
			whd->proc_stat = AIOHD_WAITIO;
			io_spunlock(whd->aiohd_lock, s);
			psema(&whd->aio_waitsema, PRIBIO|TIMER_SHIFT(AS_PHYSIO_WAIT));
			s = io_splock(whd->aiohd_lock);
		}

		for (wp = whd->aio_hdfree[KAIO_QCLEANUP]; wp;) {
			kaio_cleanup(wp);
			owp = wp->aio_next_free; /* aio_next_free will be wiped
						    out by kaio_free */
			kaio_free(wp);
			wp = owp;
		}
		for (wp = whd->aio_hdfree[KAIO_QFREE]; wp;) {
			owp = wp->aio_next_free; /* aio_next_free will be wiped
						    out by kaio_free */
			kaio_free(wp);
			wp = owp;
		}
		io_spunlock(whd->aiohd_lock, s);

		for (j = 0; j < KAIO_MAXUTABLES; j++ ) {
			if (whd->susp[j].tasks & KAIO_UNLOCKUTBL) {
				fast_unuseracc((void *)whd->susp[j].uaddr,
					       whd->maxnaio,
#ifdef EVEREST
					       ((io4ia_war && io4ia_userdma_war) ? B_LEADER : 0) |
#endif
					       B_READ | B_PHYS,
					       &(whd->susp[j].cookie));
			}
			if ((whd->susp[j].tasks & KAIO_UNMAPUTOKV)
			    && !(whd->susp[j].tasks & KAIO_DO_NOT_UNALLOC)) {
				unmaputokv(whd->susp[j].kaddr, whd->maxnaio);
			}
		}

		freesema(&whd->aio_waitsema);
		spinlock_destroy(&whd->aiohd_lock);
		spinlock_destroy(&whd->susplock);
		destroypollhead(&whd->ph);
		kmem_free(whd, sizeof(struct kaiocbhd));
	}
}

/*ARGSUSED*/
void
kaio_stats(dba_stat_t *ksbuf)/*include a bufsize arg */
{
	ksbuf->kaio_free = aio_free_list.afr_cnt;
#if 0
	kaiocb_t *fp;
	int s;

	/* global */
	ksbuf->kaio_oldaio_inuse = max_sys_aio - (long)aio_free_list.afr_cnt;
	/* Do a sanity count on free list */
	s = io_splock(aio_free_list.afr_lock);
	fp = aio_free_list.afr_head;
	ksbuf->kaio_ck_free_cnt = 0;
	while (fp) {
		ksbuf->kaio_ck_free_cnt++;
		fp = fp->aio_next_free;
	}
	io_spunlock(aio_free_list.afr_lock, s);
#endif
	return;
}
void
kaio_clearstats()
{
}

#else /* IP19 || IP25 || IP27*/
kaio_rw() {
	return ENOPKG;
}
kaio_read() {
	return ENOPKG;
}
kaio_write() {
	return ENOPKG;
}
kaio_init() {
	return;
}
kaio_done() {
	return;
}
kaio_exit() {
	return;
}
kaio_user_init() {
	return ENOPKG;
}
kaio_stats() {
	return ENOPKG;
}
kaio_suspend() {
	return ENOPKG;
}
kaio_installed() {
	return 0;
}
void kaio_getparams() {
	return;
}
void kaio_setparams(struct dba_conf *dbc)
	return;
}

#endif /* IP19 || IP25 || IP27 */
