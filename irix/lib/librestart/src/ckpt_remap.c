/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.90 $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <mutex.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/vfs.h>
#include <sys/errno.h>
#include <sys/procfs.h>
#include <sys/ckpt_procfs.h>
#include <sys/mman.h>
#include <sys/syssgi.h>
#include <sys/ckpt_sys.h>
#include <fcntl.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/cachectl.h>
#include <sys/lock.h>
#include <sys/sysmp.h>
#include <sys/prctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/usync.h>
#include <sys/pmo.h>
#include <fetchop.h>
#include <ckpt.h>
#include <ckpt_internal.h>
#include <ckpt_revision.h>
#include "librestart.h"

/*
 * The text and data here are compiled to a reserved area (0x300000 - 0x400000)
 * to avoid address conflicts with the target process.
 */

cr_t cr_low;
ckpt_saddr_t *saddrlist = NULL;
ckpt_sid_t *sidlist = NULL;
#ifdef DEBUG_FDS
ckpt_sfd_t *sfdlist = NULL;
#endif

static char execpath[CPATHLEN];

static char *UNCACHEMSG1 = "Warning: using uncached memory is unsupported "
	"on this platform";
static char *UNCACHEMSG2 = "Warning: restarting with cached memory";

static void ckpt_restore_image(ckpt_ta_t *tp);

void
ckpt_librestart_entry(ckpt_ta_t *tp)
{
	ckpt_stack_switch(ckpt_restore_image, tp);
}

void
ckpt_stack_switch(void(*func)(), ckpt_ta_t *tp)
{
	ucontext_t	ucontext;
	int zfd;
	greg_t *greg;
	/*
	 * Switch to stack in reserved memory area, then restore image
	 */
	getcontext(&ucontext);

	/*
	 * place a MAP_LOCAL memory area in low mem for stack.  Needs to
	 * be MAP_LOCAL for restoring sprocs so each share member gets
	 * a unique stack
	 */
	if ((zfd = open(DEVZERO, O_RDWR)) < 0) {
		ckpt_perror("open DEVZERO", ERRNO);
		return;
	}
	if (mmap(CKPT_MINRESERVE,
		 CKPT_STKRESERVE-CKPT_MINRESERVE,
		 PROT_READ|PROT_WRITE,
		 MAP_PRIVATE|MAP_LOCAL|MAP_FIXED,
		 zfd,
		 0) == MAP_FAILED) {
		ckpt_perror("mmap", ERRNO);
		close(zfd);
		return;
	}
	close(zfd);
	/*
	 * switch stacks
	 */
	ucontext.uc_stack.ss_sp = (char *)(CKPT_STKRESERVE - 2*sizeof(long));

	greg = ucontext.uc_mcontext.gregs;
	greg[CTX_EPC] = (greg_t)func;
	greg[CTX_T9] = (greg_t)func;		/* for PIC */
	greg[CTX_SP] = (greg_t)ucontext.uc_stack.ss_sp;
	greg[CTX_A0] = (greg_t)tp;

	setcontext(&ucontext);
	/*
	 * No return
	 */
}

/*
 * Release all address space above the checkpoint reserved area
 */
int
ckpt_relvm(ckpt_ta_t *ta, char *procpath, int doshared, ulong_t lock)
{
	int myfd;
	int i;
	int npregs;
	prmap_sgi_arg_t	maparg;
	ckpt_getmap_t	mapinfo[MAXMAP];
	int lockop;
	int nhandled;
	int nreserved;
	uid_t was;
	/*
	 * Unmap everything except the code we need to re-establish the
	 * the original mappings
	 *
	 */
	if (syssgi(SGI_CKPT_SYS, CKPT_UNMAPPRDA) < 0) {
		ckpt_perror("unmap prda", ERRNO);
		return (-1);
	}
	if (!doshared && !lock)
		return (0);

	maparg.pr_vaddr = (caddr_t)mapinfo;
	maparg.pr_size = sizeof(mapinfo);
	/*
	 * seteuid(0);
	 */
	was = 0;
	if (ckpt_seteuid(ta, &was, 1) < 0) {
		ckpt_perror("seteuid", ERRNO);
		return (-1);
	}
	if ((myfd = open(procpath, O_RDONLY)) < 0) {
		ckpt_perror("open /proc", ERRNO);
		return (-1);
	}
	if (ckpt_seteuid(ta, &was, 0) < 0) {
		ckpt_perror("seteuid", ERRNO);
		return (-1);
	}
	if (doshared) {
		/*
		 * Once we beging unmapping, can no longer ref anything outside
		 * this module
		 */
		do {
			if ((npregs = ioctl(myfd, PIOCCKPTGETMAP, &maparg)) < 0) {
				ckpt_perror("PIOCCKPTGETMAP", ERRNO);
				close(myfd);
				return(-1);
			}
			nhandled = 0;
			nreserved = 0;

			for (i = 0; i < npregs; i++) {
	
				if (IS_CKPTRESERVED(mapinfo[i].m_vaddr)) {
					nreserved++;
					continue;
				}
				if (munmap(mapinfo[i].m_vaddr, mapinfo[i].m_size) < 0) {
					ckpt_perror("munmap", ERRNO);
					close(myfd);
					return (-1);
				}
				nhandled++;
			}
			if (nreserved == MAXMAP) {
				ckpt_perror("Warning: Number of reserved >= MAXMAP", 0);
				close(myfd);
				return(-1);
			}
		} while (nhandled > 0);
	}
	if (!lock) {
		close(myfd);
		return (0);
	}
	/*
	 * If the process did ploc, get the flags set now
	 */
	while (lock) {
		if (lock & PROCLOCK) {
			lockop = PROCLOCK;
			lock &= ~(PROCLOCK|TXTLOCK|DATLOCK);
		} else if (lock & TXTLOCK) {
			lockop = TXTLOCK;
			lock &= ~TXTLOCK;
		} else if (lock & DATLOCK) {
			lockop = DATLOCK;
			lock &= ~DATLOCK;
		} else {
			ckpt_perror("ckpt_relvm:Unexpected lock value", 0);
			return (-1);
		}
		if (plock(lockop) < 0) {
			ckpt_perror("ckpt_relvm:plock failed", ERRNO);
			return (-1);
		}
	}
	/*
	 * Now unpin the resrved area.  We'll set maparg big enough to do it in
	 * one pass.
	 */
	if ((npregs = ioctl(myfd, PIOCCKPTGETMAP, &maparg)) < 0) {
		ckpt_perror("PIOCCKPTGETMAP", ERRNO);
		close(myfd);
		return(-1);
	}
	nreserved = 0;

	for (i = 0; i < npregs; i++) {
		if (!IS_CKPTRESERVED(mapinfo[i].m_vaddr))
			continue;
		if (munpin(mapinfo[i].m_vaddr, mapinfo[i].m_size) < 0) {
			ckpt_perror("ckpt_relvm:munpin failed", ERRNO);
			close(myfd);
			return (-1);
		}
		nreserved++;
	}
	if (nreserved == MAXMAP) {
		ckpt_perror("Warning: Number of reserved >= MAXMAP", 0);
		close(myfd);
		return(-1);
	}
	close(myfd);
	return (0);
}

/* ARGSUSED */
int ckpt_restore_pmi(cr_t *cp, ckpt_ta_t *tp, ckpt_magic_t magic, int *misc)
{
	pid_t mypid = *((pid_t *)misc);
	ckpt_pmilist_t  pmilist;
	size_t len;
	
	if (read(tp->ssfd, &pmilist, sizeof(pmilist)) < 0) {
		ckpt_perror("pmilist read", 0);
		return (-1);
	}
	if (pmilist.pmi_magic != magic) {
		ckpt_perror("pmilist bad magic", 0);
		return (-1);
	}
	/* at this time, all pmo/mld/mldset, and memory segments, either
	 * depend or not, are setup.
	 */
	if (vtoppid_low(pmilist.pmi_owner) != mypid) {
		return 0;
	}
	len = (caddr_t)pmilist.pmi_eaddr - (caddr_t)pmilist.pmi_vaddr;
	if(migr_range_migrate(pmilist.pmi_vaddr, len, pmilist.pmi_phandle) <0){
		ckpt_perror("migr_range_migrate", ERRNO);
		/* don't fail this. It is a optimization */
	}
	return 0;
}

static int
ckpt_restore_policy(ckpt_seg_t *segobj, int nsegs)
{
	int i;

	for (i = 0; i < nsegs; i++) {

		if (segobj[i].cs_pmhandle >= 0) {
			if (pm_attach(	segobj[i].cs_pmhandle,
					segobj[i].cs_vaddr,
					segobj[i].cs_len) < 0) {
				ckpt_perror("pm_attach", ERRNO);
				return (-1);
			}
		}
	}
	return (0);
}

static int
ckpt_restore_attrs(	ckpt_memobj_t *memobj,
			ckpt_memmap_t *memmap,
			ckpt_seg_t *segobj,
			int nsegs,
			int uncacheable,
			ulong_t lock)
{
	int i;
	size_t len;

	for (i = 0; i < nsegs; i++) {

		if (segobj[i].cs_prots != memmap->cm_maxprots) {
			/*
			 * In case of stack, adjust length down
			 * 1 byte to avoid user space violation
			 * checks in kernel.  This is innocuos
			 * since it doesn't change the number
			 * of pages
			 */
			len = segobj[i].cs_len;
			if (memmap->cm_flags & CKPT_STACK)
				len -= 1;

			if (mprotect(	segobj[i].cs_vaddr,
					len,
					segobj[i].cs_prots) < 0) {
				/*
				 * Report an error and die!
				 */
				ckpt_perror("mprotect", ERRNO);
				return (-1);
			}
		}
		if ((segobj[i].cs_notcached == CKPT_NONCACHED)&&
		    !(memmap->cm_xflags&CKPT_PHYS)) {
			/*
			 * Some systems don't allow setting this!
			 */
			if (uncacheable) {
				ckpt_perror(UNCACHEMSG1, 0);
				ckpt_perror(UNCACHEMSG2, 0);
			} else {
				if (cacheflush(	segobj[i].cs_vaddr,
						(int)segobj[i].cs_len,
						BCACHE) < 0) {
					ckpt_perror("cachectl cache flush", ERRNO);
					return (-1);
				}
				if (cachectl(	segobj[i].cs_vaddr,
						(int)segobj[i].cs_len,
						UNCACHEABLE) < 0) {
					ckpt_perror("cachectl", ERRNO);
					return (-1);
				}
			}
		}
		/*
		 * Do locking
		 */
		while (segobj[i].cs_lockcnt-- > 0) {
			if (mpin(segobj[i].cs_vaddr, segobj[i].cs_len) < 0) {
				ckpt_perror("mpin", ERRNO);
				return (-1);
			}
		}
						
	}
	if ((lock & DATLOCK) &&
	    ((memmap->cm_xflags & CKPT_BREAK)||(memmap->cm_flags & CKPT_STACK))) {
		/*
		 * Special case brk and stack areas if DATLOCK set
		 *
		 * When we did the sbrk with DATLOCK set, a lock got
		 * placed by kernel auto lock.  So, subtract one for
		 * that lock.
		 */
		void *vaddr;
		size_t len;

		if (memmap->cm_flags & CKPT_STACK) {
			vaddr = memmap->cm_vaddr;
			len = memmap->cm_len;
		} else {
			vaddr = memobj->cmo_brkbase;
			len = memobj->cmo_brksize;
		}
		if (len) {
			if (munpin(vaddr, len) < 0) {
				ckpt_perror("munpin", ERRNO);
				return (-1);
			}
		}
	}
	return (0);
}
/*
 * Read a contiguous section from the image file to memory
 */
static int
ckpt_read_range(ckpt_ta_t *tp, caddr_t vaddr, size_t len)
{
	int mfd = tp->mfd;
	iovec_t	iov[MAXIOV];
	int iovcnt;

	while (len > 0) {

		for (iovcnt = 0; iovcnt < MAXIOV && len > 0; iovcnt++) {

			iov[iovcnt].iov_base  = vaddr;
			iov[iovcnt].iov_len =  min(len, CKPT_IOBSIZE);

			len -= iov[iovcnt].iov_len;
			vaddr += iov[iovcnt].iov_len;
		}
		if (readv(mfd, iov, iovcnt) < 0) {
#ifdef DEBUG_READV
			ckpt_dump_iov(iov, iovcnt);
#endif
			ckpt_perror("read range", ERRNO);
			return (-1);
		}
	}
	return (0);
}
/*
 * Read a section of fetchop variables from the image file to memory
 */
static int
ckpt_read_fetchop(ckpt_ta_t *tp, caddr_t *vaddr, size_t len)
{
	int mfd = tp->mfd;
	char *buf;
	ssize_t bufsiz;
	ssize_t buflen;
	fetchop_var_t *src;
	fetchop_var_t *dst;

	bufsiz = min(len, CKPT_IOBSIZE);
	buf = ckpt_malloc(bufsiz);

	dst = (fetchop_var_t *)vaddr;

	while (len) {

		buflen = (ssize_t)min(len, bufsiz);

		if (read(mfd, buf, buflen) < 0) {
			ckpt_perror("read fetchop", ERRNO);
			ckpt_free(buf);
			return (-1);
		}
		src = (fetchop_var_t *)buf;

		len -= buflen;

		while (buflen) {
			*dst = *src;

			src = (fetchop_var_t *)((char *)src + FETCHOP_VAR_SIZE);
			dst = (fetchop_var_t *)((char *)dst + FETCHOP_VAR_SIZE);

			buflen -= FETCHOP_VAR_SIZE;
		}
	}
	ckpt_free(buf);
	return (0);
}
		
static int
ckpt_read_image(ckpt_ta_t *tp,
		ckpt_memmap_t *memmap,
		ckpt_seg_t *segobj,
		int nsegs)
{
	long i;
	long npages;
	caddr_t *addrlist;
	int sfd = tp->sfd;
	int mfd = tp->mfd;
	iovec_t	iov[MAXIOV];
	caddr_t base;
	size_t len;
	int iovcnt;

	if ((memmap->cm_flags & CKPT_PRDA)&&(memmap->cm_tid != 0)) {
		/*
		 * Don't restore held signal mask.  Need to keep all
		 * signals held until cpr reatores context.  The hold mask
		 * will get restored as paert of the context restoration.
		 */
		char *ptr;

		len = (size_t)((char *)&PRDA->t_sys.t_hold - (char *)PRDA);
		ptr = (char *)PRDA;

		if (read(mfd, ptr, len) < 0) {
			ckpt_perror("read restore pthread prda", ERRNO);
			return (-1);
		}
		ptr += len;

		lseek(mfd, (off_t)sizeof(k_sigset_t), SEEK_CUR);
		ptr += sizeof(k_sigset_t);

		len = memmap->cm_len - len - sizeof(k_sigset_t);

		if (read(mfd, ptr, len) < 0) {
			ckpt_perror("read restore pthread prda", ERRNO);
			return (-1);
		}
	} else if (memmap->cm_cflags & CKPT_SAVEALL) {

		if (ckpt_read_range(tp, memmap->cm_vaddr, memmap->cm_savelen) < 0)
			return (-1);

	} else if (memmap->cm_cflags & CKPT_SAVEMOD) {

	        if ((npages = memmap->cm_nmod) == 0)
			return (0);

		addrlist = (caddr_t *)ckpt_malloc(npages * (int)sizeof(caddr_t));

		if (addrlist == NULL)
			return (-1);

		if (read(sfd, addrlist, npages * sizeof(caddr_t)) != npages * sizeof(caddr_t)) {
			ckpt_perror("read addrlist", ERRNO);
			ckpt_free(addrlist);
			return (-1);
		}
		iovcnt = 0;
		base = addrlist[0];
		len = memmap->cm_pagesize;

		for (i = 1; i < npages; i++) {

			if ((addrlist[i] == (base + len))&&(len < CKPT_IOBSIZE))

				len += memmap->cm_pagesize;

			else {
				iov[iovcnt].iov_base = base;
				iov[iovcnt].iov_len = len;
				iovcnt++;

				if (iovcnt == MAXIOV) {

					if (readv(mfd, iov, iovcnt) < 0) {
#ifdef DEBUG_READV
						ckpt_dump_iov(iov, iovcnt);
#endif
						ckpt_perror("readv", ERRNO);
						ckpt_free(addrlist);
						return (-1);
					}
					iovcnt = 0;
				}
				base = addrlist[i];
				len = memmap->cm_pagesize;
			}
		}
		iov[iovcnt].iov_base = base;
		iov[iovcnt].iov_len = len;
		iovcnt++;

		if (readv(mfd, iov, iovcnt) < 0) {
#ifdef DEBUG_READV
			ckpt_dump_iov(iov, iovcnt);
#endif
			ckpt_perror("readv", ERRNO);
			ckpt_free(addrlist);
			return (-1);
		}
		ckpt_free(addrlist);

	} 

	if (memmap->cm_xflags & CKPT_FETCHOP) {
		/*
		 * Find and setup fetchop segments in this mapping
		 */
		for (i = 0; i < nsegs; i++) {
			if (segobj[i].cs_notcached == CKPT_NONCACHED_FETCHOP) {
				if (syssgi(	SGI_FETCHOP_SETUP,
						segobj[i].cs_vaddr,
						segobj[i].cs_len) < 0) {
                        	        ckpt_perror("fetchop setup", ERRNO);
                                	return (-1);
				}
				if (mpin(	segobj[i].cs_vaddr,
						segobj[i].cs_len) < 0) {
					ckpt_perror("fetchop mpin", ERRNO);
					return (-1);
				}
				segobj[i].cs_lockcnt--;

				if (ckpt_read_fetchop(	tp,
							segobj[i].cs_vaddr,
							segobj[i].cs_len) < 0)
					return (-1);
                        }
		}
	}
	return (0);
}


static void
ckpt_restore_stack(ckpt_memmap_t *memmap, ucontext_t *ucp)
{
	extern void ckpt_grow_stack(void *);

	ckpt_grow_stack(memmap->cm_vaddr);

	setcontext(ucp);
}

/*
 * ckpt_stackbld - take advantage of setcontexts setting of p_stkbase
 * inside the kernel to create a stack region
 *
 * Warning - very machine dependent code below
 */
static int
ckpt_stackbld(ckpt_memmap_t *memmap)
{
	ucontext_t ccontext;	/* current context */
	ucontext_t rcontext;	/* restore context */
	int rval;
	volatile int seq = 0;
	greg_t *greg;
	/*
	 * Get current context so we can return
	 */
	rval = getcontext(&ccontext);
	if (seq++ > 0) {
		/*
		 * back on original context
		 */
		return (0);
	}
	if (rval < 0) {
		ckpt_perror("getcontext ccontext", ERRNO);
		return (-1);
	}
	/*
	 * Get a context for switching stacks
	 */
	if (getcontext(&rcontext) < 0) {
		ckpt_perror("getcontext rcontext", ERRNO);
		return (-1);
	}
	/*
	 * Calc max stack addr
	 */
	rcontext.uc_stack.ss_sp = (char *)memmap->cm_vaddr + memmap->cm_len;
	rcontext.uc_stack.ss_size = 0;
	/*
	 * Set up context to call ckpt_restore_stack
	 */
	greg = rcontext.uc_mcontext.gregs;
	greg[CTX_EPC] = (greg_t)ckpt_restore_stack;
	greg[CTX_T9] = (greg_t)ckpt_restore_stack;	/* for PIC */
	greg[CTX_SP] = (greg_t)rcontext.uc_stack.ss_sp;
	greg[CTX_A0] = (greg_t)memmap;
	greg[CTX_A1] = (greg_t)&ccontext;

	setcontext(&rcontext);
	/*
	 * No return on succeess
	 */
	ckpt_perror("setcontext", ERRNO);
	return (-1);
}
/*
 * Deal with special devices
 *
 * Really ought to be in ckpt_special.c, but need to live in reserved addr
 * range.
 */
/* ARGSUSED */
static int
ckpt_restore_special(ckpt_ta_t *tp, ckpt_memmap_t *memmap, int sfd)
{
	int memfd;
	struct stat sb;

	if ((xstat(_STAT_VER, DEVCCSYNC, &sb) >= 0) &&
	    (memmap->cm_rdev == sb.st_dev)) {
		ckpt_ccsync_t ccsync;
		void *pa;
		union {
			double d;
			u_int8_t u8;
			__uint64_t u64;
		} value;

		if (read(sfd, &ccsync, sizeof(ccsync)) < 0) {
			ckpt_perror("read", ERRNO);
			return (-1);
		}
		if ((memfd = ccsync.cc_fd) < 0) {
			if ((memfd = open(DEVCCSYNC, O_RDWR)) < 0) {
				ckpt_perror("open ccsync", ERRNO);
				return (-1);
			}
		}
		/*
		 * Check mustrun.  If driver didn't lock us, then we had
		 * no mustrun state before driver mmap...so in that case
		 * undo mustrun so driver is free to move us.
		 */
		if (!(ccsync.cc_info.ccsync_flags & CCSYNC_MRLOCKONLY)) {
			if (sysmp(MP_RUNANYWHERE) < 0) {
				ckpt_perror("runanywhere", ERRNO);
				return (-1);
			}
		}
		if (mmap(	memmap->cm_vaddr,
				memmap->cm_len,
				memmap->cm_maxprots,
				memmap->cm_mflags|MAP_FIXED,
				memfd,
				memmap->cm_foff) == (void *)-1L) {
			ckpt_perror("mmap ccsync", ERRNO);
			return (-1);
		}
		/*
		 * Now set counter value
		 */
		pa = (void *)((char *)memmap->cm_vaddr + EVC_OFFSET);

		/* read */
		value.d = *((volatile double *)pa);
		value.u8 &= 0x3f;

		while (value.u8 != ccsync.cc_info.ccsync_value) {
			/* inc */
			value.u64 = 0xffff;
			*((volatile double *)pa) = value.d;
			/* read */
			value.d = *((volatile double *)pa);
			value.u8 &= 0x3f;
		}
		if (ccsync.cc_fd < 0)
			close(memfd);

	} else {
		ckpt_perror("unexpected device type", 0);
		return (-1);
	}
	return (0);
}

/* ARGSUSED */
int
ckpt_restore_pusema(cr_t *cp, ckpt_ta_t *tp, ckpt_magic_t magic, int *misc)
{
	ckpt_pusema_t pusema;
	usync_arg_t uarg;
	pid_t mypid = *((pid_t *)misc);

	if (ckpt_rxlate_pusema(cp->cr_pci.pci_revision, tp->ssfd, &pusema) < 0)
		return (-1);

	if (pusema.pu_magic != magic) {
		ckpt_perror("pusema bad magic", 0);
		return (-1);
	}
	if (vtoppid_low(pusema.pu_owner) == mypid) {
		/*
		 * Since no one is going to actual wait for the sema
		 * until we get through this fuss, adjust prepost by handoff
		 * value.  It appears to be possible for prepost to be less
		 * than handoff, in which case we'll need to consume some
		 */
		pusema.pu_usync.value -= pusema.pu_usync.handoff;

		while (pusema.pu_usync.value > 0) {

			uarg.ua_version = USYNC_VERSION_2;
			uarg.ua_addr = (__uint64_t)pusema.pu_uvaddr;
			uarg.ua_policy = pusema.pu_usync.policy;
			uarg.ua_flags = 0;

			if (usync_cntl(USYNC_UNBLOCK, &uarg) < 0) {
				ckpt_perror("usync unblock", ERRNO);
				return (-1);
			}
			pusema.pu_usync.value--;
		}
		/*
		 * To reestablish the handoff, we'll set the handoff addr
		 * to this usma and set a timeout of 0, so primary
		 * object address doesn't matter, as long as it's different
		 * than pusema.pu_uvaddr.  Expect ETIMEDOUT.
		 */
		while (pusema.pu_usync.handoff > 0) {
			int err;

			uarg.ua_version = USYNC_VERSION_2;
			uarg.ua_addr = (__uint64_t)&pusema;
			uarg.ua_handoff = (__uint64_t)pusema.pu_uvaddr;
			uarg.ua_policy = pusema.pu_usync.policy;
			uarg.ua_flags = USYNC_FLAGS_TIMEOUT;
			uarg.ua_sec = 0;
			uarg.ua_nsec = 0;

			err = usync_cntl(USYNC_HANDOFF, &uarg);

			if (err == 0)
				ERRNO = ECKPT;
			if (ERRNO != ETIMEDOUT) {
				ckpt_perror("usync unblock", ERRNO);
				return (-1);
			}
			pusema.pu_usync.handoff--;
		}
		/*
		 * Consume extra prepost
		 */
		while (pusema.pu_usync.value < 0) {

			uarg.ua_version = USYNC_VERSION_2;
			uarg.ua_addr = (__uint64_t)pusema.pu_uvaddr;
			uarg.ua_policy = pusema.pu_usync.policy;
			uarg.ua_flags = 0;

			if (usync_cntl(USYNC_INTR_BLOCK, &uarg) < 0) {
				ckpt_perror("usync unblock", ERRNO);
				return (-1);
			}
			pusema.pu_usync.value++;
		}
	}
	if (pusema.pu_usync.notify &&
	    vtoppid_low(pusema.pu_usync.notify) == mypid) {

		uarg.ua_version = USYNC_VERSION_2;
		uarg.ua_addr = (__uint64_t)pusema.pu_uvaddr;
		uarg.ua_policy = USYNC_POLICY_FIFO;

		if (usync_cntl(USYNC_NOTIFY_REGISTER, &uarg) < 0) {
			ckpt_perror("usync unblock", ERRNO);
			return (-1);
		}
	}
	return (0);
}
/* ARGSUSED */
int
ckpt_restore_prattach(cr_t *cp, ckpt_ta_t *tp, ckpt_magic_t magic, int *misc)
{
	ckpt_prattach_t prattach;
	struct prattach_args_t attach;
	pid_t mypid = *((pid_t *)misc);

	if (read(tp->ssfd, &prattach, sizeof(prattach)) < 0) {
		ckpt_perror("prattach read", ERRNO);
		return (-1);
	}
	if (prattach.pr_magic != magic) {
		ckpt_perror("prattach bad magic", 0);
		return (-1);
	}
	if (vtoppid_low(prattach.pr_owner) == mypid)
		if (unblockproc(vtoppid_low(prattach.pr_attach)) < 0) {
			ckpt_perror("prattach unblockproc", ERRNO);
			return (-1);
		}
	if (vtoppid_low(prattach.pr_attach) != mypid)
		return (0);

	/*
	 * wait for the owner to be ready
	 */
	if (blockproc(mypid) < 0) {
		ckpt_perror("prattach blockproc", ERRNO);
		return (-1);
	}
	/*
	 * do the attach
	 */
	attach.local_vaddr = (__int64_t)prattach.pr_localvaddr;
	attach.vaddr = (__int64_t)prattach.pr_vaddr;
	attach.pid = vtoppid_low(prattach.pr_owner);
	attach.prots = prattach.pr_prots;
	attach.flags = PRATTACH_ALIGN;

	if (prctl(PR_ATTACHADDRPERM, &attach, NULL) < 0) {
		ckpt_perror("prattach attach", ERRNO);
		return (-1);
	}
	return (0);
}
/*
 * Inherited shared memory sync
 *
 * Sync up after all memory has been re-mapped so that procs that need
 * to dup a region off of another proc are asured that the region exists
 *
 * We come here after all conventional mappings are complete, we unblock any
 * proc that depends on our mappings and then we block ourselves waiting
 * for procs we depend upon
 */
/* ARGSUSED */
int
ckpt_restore_depend(cr_t *cp, ckpt_ta_t *tp, ckpt_magic_t magic, int *misc)
{
	pid_t mypid = *((pid_t *)misc);
	ckpt_depend_t depend;

	if (magic != CKPT_MAGIC_DEPEND) {
		ckpt_perror("magic wrong", 0);
		return (-1);
	}
	if (read(tp->ssfd, &depend, sizeof(depend)) < 0) {
		ckpt_perror("group file read", ERRNO);
		return (-1);
	}
	if (depend.dp_magic != magic) {
		ckpt_perror("depend bad magic", ERRNO);
		return (-1);
	}
	/*
	 * Source ready; wake up its dependents
	 */
	if (vtoppid_low(depend.dp_spid) == mypid) {
		if (unblockproc(vtoppid_low(depend.dp_dpid)) < 0) {
			ckpt_perror("depend unblockproc", ERRNO);
			return (-1);
		}
	}
	if (vtoppid_low(depend.dp_dpid) != mypid)
		return (0);
	/*
	 * Wait for the source to be ready
	 */
	if (blockproc(mypid) < 0) {
		ckpt_perror("depend blockproc", ERRNO);
		return (-1);
	}
	return (0);
}

static int
ckpt_restore_ismmem(ckpt_ta_t *ta, ckpt_memmap_t *memmap)
{
	static char *procprefix = "/proc/";
	int pid;
	char pidbuf[16];
	char procbuf[24];
	int len;
	uid_t was;
	int procfd;
	ckpt_forkmem_arg_t arg;
	/*
	 * format /proc file name
	 */
	pid = memmap->cm_pid;

	len = sizeof(pidbuf) - 1;
	pidbuf[len--] = 0;

	while (pid != 0) {
		pidbuf[len--] = '0' + (char)(pid % 10);
		pid = pid/10;
	}
	procbuf[0] = 0;
	ckpt_strcat(procbuf, procprefix);
	ckpt_strcat(procbuf, &pidbuf[len+1]);
	/*
	 * upgrade identity
	 */
	was = 0;
	if (ckpt_seteuid(ta, &was, 1) < 0) {
		ckpt_perror("seteuid", ERRNO);
		return (-1);
	}
	/*
	 * open /proc
	 */
	procfd = open(procbuf, O_RDONLY);
	if (procfd < 0) {
		ckpt_perror("proc open", ERRNO);
		return (-1);
	}
	if (ckpt_seteuid(ta, &was, 0) < 0) {
		ckpt_perror("seteuid", ERRNO);
		return (-1);
	}
	/*
	 * map in the memory
	 */
	arg.vaddr = memmap->cm_vaddr;
	arg.len = (ssize_t)memmap->cm_len;
	arg.local = memmap->cm_mflags & MAP_LOCAL;

	if (ioctl(procfd, PIOCCKPTFORKMEM, &arg) < 0) {
		ckpt_perror("/proc PIOCCKPTFORKMEM", ERRNO);
		(void)close(procfd);
		return (-1);
	}
	(void)close(procfd);
	return (0);
}

static int
ckpt_restore_shmem(ckpt_memmap_t *memmap, int sfd)
{
	ckpt_shmlist_t *shml = (ckpt_shmlist_t *)CKPT_SHMRESERVE;
	ulong_t i;
	key_t key;
	int shmflg;
	int shmid = -1;
	int rmid;
	struct shmid_ds shmds;
	size_t segsz;
	char errbuf[80];

	if (memmap->cm_cflags & CKPT_DEPENDENT) {
		/*
		 * another proc restored the shared mem, look it up
		 * and attach it
		 */
		for (i = 0; i < shml->cshm_count; i++) {
			if (shml->cshm_map[i].mapid == memmap->cm_mapid) {
				shmid = shml->cshm_map[i].shmid;
				break;
			}
		}
		if (shmid == -1) {
			ckpt_perror("shmem not found", 0);
			return (-1);
		}
	} else {
		/*
		 * we need to create the mem
		 */
		i = ckpt_test_then_add(&shml->cshm_count, 1);

		if (memmap->cm_auxptr) {
			if (read(sfd, &shmds, sizeof(shmds)) < 0) {
				ckpt_perror("shmds read", ERRNO);
				return (-1);
			}
			key = shmds.shm_perm.key;
			shmflg = (int)shmds.shm_perm.mode;
			segsz = shmds.shm_segsz;
			rmid = 0;
		} else {
			key = IPC_PRIVATE;
			shmflg = S_IRWXU|S_IRWXG|S_IRWXO;
			segsz = memmap->cm_len;
			rmid = 1;
		}
		shmid = shmget_id(key, segsz, shmflg|IPC_CREAT, memmap->cm_shmid);
		if (shmid == -1) {
			if (ERRNO == ENOSPC) {
				ckpt_strcpy(errbuf, "sys V shared memory restore failed for shmid: ");
				ckpt_itoa(&errbuf[ckpt_strlen(errbuf)], memmap->cm_shmid);
				ckpt_strcat(errbuf, ": id in use");
				ckpt_perror(errbuf, 0);
			} else
				ckpt_perror("shmget", ERRNO);
			return (-1);
		}
		shml->cshm_map[i].mapid = memmap->cm_mapid;
		shml->cshm_map[i].shmid = shmid;
		shml->cshm_map[i].rmid = rmid;
	}
	if (memmap->cm_maxprots & PROT_WRITE)
		shmflg =0;
	else
		shmflg = SHM_RDONLY;

	if (shmat(shmid, memmap->cm_vaddr, shmflg) != memmap->cm_vaddr) {
		ckpt_perror("shmat", ERRNO);
		return (-1);
	}
	return (0);
}

/*
 * ckpt_adjust_execargs
 *
 * don't want to cover brkbase with any mapping
 */
static void
ckpt_adjust_execargs(ckpt_execargs_t *args, ckpt_memobj_t *memobj)
{
      if ((args->vaddr + args->len) > memobj->cmo_brkbase) {
              args->len = memobj->cmo_brkbase - (caddr_t)args->vaddr;
              args->zfodlen = 0;

              return;
      }
      if ((args->vaddr + args->len + args->zfodlen) > memobj->cmo_brkbase)  {
              args->zfodlen = memobj->cmo_brkbase - (args->vaddr + args->len);
              return;
      }
}

static int
ckpt_restore_memmap(	ckpt_ta_t *tp,
			ckpt_memobj_t *memobj,
			ckpt_memmap_t *memmap,
			int sfd,
			int *brk)
{
	int memfd;
	int oflags;
	struct stat sb;
	ckpt_execargs_t execargs;
	static int setexecvnode = 0;

	if (memmap->cm_xflags & CKPT_PRATTACH)
		/*
		 * Attach already done
		 */
		return (0);

	if (memmap->cm_xflags & CKPT_ISMEM) 
		return (ckpt_restore_ismmem(tp, memmap));

	if (memmap->cm_flags & CKPT_PRDA) {
		/*
	 	 * No need to reference it here.  The first ref from restoring
		 * the contents will do.
		 */
		return (0);
	}
	if (memmap->cm_flags & CKPT_STACK)
		return (ckpt_stackbld(memmap));

	if (memmap->cm_xflags & CKPT_SPECIAL)
		return (ckpt_restore_special(tp, memmap, sfd));

	if (memmap->cm_xflags & CKPT_SHMEM)
		return (ckpt_restore_shmem(memmap, sfd));

	if ((memfd = memmap->cm_fd) < 0) {
		/* open file with modes consistent with mapping */
	
		switch (memmap->cm_maxprots&(PROT_READ|PROT_WRITE)) {
		case PROT_READ:
			oflags = O_RDONLY;
			break;
		case PROT_WRITE:
			oflags = O_WRONLY;
			break;
		case PROT_READ|PROT_WRITE:
			if ((memmap->cm_mflags & MAP_PRIVATE)||
			    (memmap->cm_flags & CKPT_EXECFILE))
				oflags = O_RDONLY;
			else
				oflags = O_RDWR;
			break;
		}
		if (memmap->cm_flags & (CKPT_MAPFILE|CKPT_EXECFILE)) {
			memfd = ckpt_open(tp, memmap->cm_path, oflags, 0);
			/*
			 * Avoid ref to errno here, just try chmod if open fails
			 */
			/*
			 * Yes, this breaks rules by programming directly to
			 * the xstat interface!
			 */
			if ((memfd < 0) &&
			    (xstat(_STAT_VER, memmap->cm_path, &sb) >= 0) &&
			    (chmod(memmap->cm_path, S_IRWXU) >= 0)) {
				memfd = ckpt_open(tp, memmap->cm_path, oflags, 0);
				(void)chmod(memmap->cm_path, sb.st_mode);
			}
			if (	setexecvnode == 0 &&
				(memmap->cm_mflags & MAP_PRIMARY) &&
				(tp->pi.cp_stat != SZOMB)) {
				ckpt_strcpy(execpath, memmap->cm_path);
				setexecvnode = 1;
			}
		} else
			memfd = open(DEVZERO, oflags);

		if (memfd < 0) {
			/*
			 * Report an error and die!
			 */
			ckpt_perror("ckpt_restore_memmap: open", ERRNO);
			return (-1);
		}
	}
	if (memmap->cm_flags & CKPT_EXECFILE) {

		execargs.fd = memfd;
		execargs.vaddr = memmap->cm_vaddr;
		execargs.len = memmap->cm_maxoff - memmap->cm_foff;
		execargs.off = memmap->cm_foff;
		if (memmap->cm_mflags & MAP_PRIVATE) {
			execargs.prot = memmap->cm_maxprots;
			execargs.zfodlen = memmap->cm_len - (memmap->cm_maxoff -
							memmap->cm_foff);
		} else {
			execargs.prot = 0;
			execargs.zfodlen = 0;
		}
		execargs.flags = memmap->cm_mflags;

                if (memmap->cm_xflags & CKPT_BREAK) {
                        /*
                         * Adjust execargs for brk value
                         */
                        ckpt_adjust_execargs(&execargs, memobj);
                }
		if (syssgi(SGI_CKPT_SYS, CKPT_EXECMAP, &execargs) < 0) {
			ckpt_perror("CKPT_EXECMAP", ERRNO);
			return (-1);
		}
		/*
		 * If brkbase, brk now
		 */
		if ((memmap->cm_xflags & CKPT_BREAK) && (*brk == 0)) {
	
			setbrk(memobj->cmo_brkbase);
	
			if (syssgi(SGI_CKPT_SYS, CKPT_SETBRK, memobj->cmo_brkbase) < 0) {
				ckpt_perror("CKPT_SETBRK", ERRNO);
				return (-1);
			}
			if ((long)sbrk((ssize_t)memobj->cmo_brksize) < 0) {
				ckpt_perror("sbrk", ERRNO);
				return (-1);
			}
			*brk = 1;
		}
	} else {
		/*
		 * mapped files and /dev/zero anon mappings
		 */
		/*
		 * Check if this mapping is sitting on top of the brk area.
		 * If it is and brk has not been done, do it now, then map this
		 * file
		 */
		if ((memmap->cm_xflags & CKPT_BREAK)&&(*brk == 0)) {

			setbrk(memobj->cmo_brkbase);
		
			if (syssgi(SGI_CKPT_SYS, CKPT_SETBRK, memobj->cmo_brkbase) < 0) {
				ckpt_perror("CKPT_SETBRK", ERRNO);
				return (-1);
			}
			if ((long)sbrk((ssize_t)memobj->cmo_brksize) < 0) {
				ckpt_perror("sbrk", ERRNO);
				return (-1);
			}
			*brk = 1;
		}
		if ((long)mmap(memmap->cm_vaddr,
				memmap->cm_len,
				memmap->cm_maxprots,
				memmap->cm_mflags|MAP_FIXED,
				memfd,
				memmap->cm_foff) < 0) {
			/*
			 * Report an error and die!
			 */
			ckpt_perror("mmap", ERRNO);
			return (-1);
		}	 
	}
	if (memmap->cm_fd < 0)
		(void)close(memfd);
	return (0);
}

/*
 * Do final restoration of uid/gid, rlimits, timrers, threads.
 * Only restore uid/gid for zombies
 */
static void
ckpt_restore_context(	int cprfd,
			pid_t mypid,
			ckpt_ta_t *ta,
			struct rlimit *rlp,
			struct itimerval *itp)
{
	int i;
	char status = 0;

	if (ckpt_setroot(ta, mypid) < 0)
		return;

	if (ta->pi.cp_stat != SZOMB) {
		for (i = 0; i < RLIM_NLIMITS; i++) {
			switch (i) {
			case RLIMIT_FSIZE:
			case RLIMIT_DATA:
			case RLIMIT_NOFILE:
			case RLIMIT_VMEM:
			case RLIMIT_RSS:
			case RLIMIT_STACK:
#if (RLIMIT_AS != RLIMIT_VMEM)
			case RLIMIT_AS:
#endif
				if (setrlimit(i, &rlp[i])) {
					ckpt_perror("setrlmit", ERRNO);
					return;
				}
				break;
			default:
				break;
			}
		}
	}
	/*
	 * Finalize uid switch now
	 */
	if (ckpt_restore_identity_late(ta, mypid))
		return;

	if (ta->pi.cp_stat != SZOMB) {
		/*
		 * Restore timers
		 */
		for (i = 0; i < ITIMER_MAX; i++) {
			if (timerisset(&itp[i].it_value)) {
				if (setitimer(i, &itp[i], NULL)) {
					ckpt_perror("setitimer", ERRNO);
					return;
				}
			}
		}
		/*
		 * Restore sigactions
		 */
		for (i = 0; i < ta->sighdr.cs_nsigs; i++) {
			if (ksigaction( i+1, &ta->sap[i], NULL,
			      (void (*)(int ,...))ta->sighdr.cs_sigtramp) < 0) {
				ckpt_perror("sigaction\n", ERRNO);
				return;
			}
		}
		/*
		 * Restore threads
		 */
	}
	/*
	 * Let our cprfd know we're ready.  It will restore our machine 
	 * context.
	 */
	if (ta->pi.cp_stat == SZOMB)
                ckpt_update_proclist_states(ta->pindex, CKPT_PL_ZOMBIE);

	ckpt_update_proclist_states(ta->pindex, CKPT_PL_DONE);
	if (write(cprfd, &status, 1) < 0) {
		ckpt_perror("status write failed", ERRNO);
		return;
	}
	(void)close(ta->ifd);
	(void)close(ta->sfd);
	(void)close(cprfd);

	if (ta->pi.cp_stat == SZOMB) {
		/*
		 * don't exit until parent is restored
		 */
		if (blockproc(mypid) < 0) {
			ckpt_perror("blockproc", ERRNO);
			return;
		}
	}
	(void)close(*errfd_p);
#ifdef DEBUG_FDS
	ckpt_scan_fds(ta);
#endif

	if (ckpt_exit_barrier()) {
		/*
		 * We're the last one though on this address space.
		 * Clean up any shared lists.  These have semaphores attached,
		 * so we want to get rid of them so sema can be reclaimed
		 */
		if (saddrlist)
				munmap(saddrlist, saddrlist->saddr_size);
		if (sidlist)
				munmap(sidlist, sidlist->sid_size);
#ifdef DEBUG_FDS
		if (sfdlist)
				munmap(sfdlist, sfdlist->sfd_size);
#endif
	}
	ckpt_pthread0_check();

	restartreturn( -ta->blkcnt,
			ta->pi.cp_psi.ps_xstat,
			ta->pi.cp_stat == SZOMB);
}

int
ckpt_restore_memobj(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	if (read(tp->sfd, &tp->memobj, sizeof(ckpt_memobj_t)) < 0) {
		ckpt_perror("read memobj %s)\n", ERRNO);
		return (-1);
	}
	if (tp->memobj.cmo_magic != magic) {
		ckpt_perror("memobj magic mismatch, got",tp->memobj.cmo_magic);
		return (-1);
	}
	tp->memmap_offset = lseek(tp->sfd, 0, SEEK_CUR);
	return (0);
}
/*
 * Skip the contents of an memmap
 */
static void
ckpt_skip_memmap(ckpt_ta_t *tp, ckpt_memmap_t *memmap)
{
	int sfd = tp->sfd;
	int mfd = tp->mfd;

	lseek(sfd, (off_t)(memmap->cm_nsegs*sizeof(ckpt_seg_t)), SEEK_CUR);
	lseek(sfd, (off_t)(memmap->cm_auxsize), SEEK_CUR);
	
	if (memmap->cm_cflags & CKPT_SAVEALL)

		lseek(mfd, (off_t)memmap->cm_savelen, SEEK_CUR);

	else if (memmap->cm_cflags & CKPT_SAVEMOD) {

		lseek(sfd, (off_t)(memmap->cm_nmod*sizeof(caddr_t)), SEEK_CUR);
		lseek(mfd, (off_t)(memmap->cm_nmod*memmap->cm_pagesize), SEEK_CUR);
	}
}

int
ckpt_restore_mappings(	ckpt_ta_t *tp,
			pid_t  mypid,
			tid_t tid,
			int local_only,	/* individual pthreads set this flag */
			int *depend,
			int *prattach,
			int *brk)
{
	cr_t *crp = tp->crp;
	int sfd = tp->sfd;
	int mfd = tp->mfd;
	int nmaps = tp->memobj.cmo_nmaps;
	int i;
	int nsegs;
	ckpt_memmap_t memmap;
	ckpt_seg_t *segobj;

	lseek(sfd, tp->memmap_offset, SEEK_SET);
	lseek(mfd, (off_t)0, SEEK_SET);

	for (i = 0; i < nmaps; i++) {
		if (ckpt_rxlate_memmap(crp->cr_pci.pci_revision, sfd, &memmap) < 0)
			return (-1);

		if (memmap.cm_magic != CKPT_MAGIC_MEMMAP) {
			ckpt_perror("cm_magic", 0);
			return (-1);
		}
		if (IS_CKPTRESERVED(memmap.cm_vaddr))
			/*
			 * Don't restore checkpoint memory!
			 */
			continue;

		if (local_only) {
			/*
			 * only do local mappings where tid matches ours
			 */
			if (((memmap.cm_mflags & MAP_LOCAL) == 0)||
		    	    (memmap.cm_tid != tid)) {
				ckpt_skip_memmap(tp, &memmap);
				continue;
			}
		} else {
			/*
			 * skip local mappings that don't match our tid
			 */
			if ((memmap.cm_mflags & MAP_LOCAL)&&
		    	    (memmap.cm_tid != tid)) {
				ckpt_skip_memmap(tp, &memmap);
				continue;
			}
		}
		if ((memmap.cm_cflags & CKPT_DEPENDENT) && !*depend) {
			*depend = 1;
			if (ckpt_read_share_property_low(
						tp,
						CKPT_MAGIC_DEPEND, 
						(void *)&mypid) < 0)
				return (-1);
		}
		if ((memmap.cm_xflags & CKPT_PRATTACH) && !*prattach) {
			*prattach = 1;
			if (ckpt_read_share_property_low(
						tp,
						CKPT_MAGIC_PRATTACH, 
						(void *)&mypid) < 0)
				return (-1);
		}
		if (ckpt_restore_memmap(tp, &tp->memobj, &memmap, sfd, brk) < 0)
			return (-1);

		nsegs = memmap.cm_nsegs;

		segobj = (ckpt_seg_t *)ckpt_malloc(nsegs*(int)sizeof(ckpt_seg_t));
		if (segobj == NULL) {
			ckpt_perror("ckpt_malloc", ERRNO);
			return (-1);
		}
		if (ckpt_rxlate_segobjs(crp->cr_pci.pci_revision, sfd, segobj, nsegs) < 0) {
			ckpt_free(segobj);
			return (-1);
		}
		/*
		 * Attach policy modules before faulting in memory
		 */
		if (ckpt_restore_policy(segobj, nsegs) < 0) {
			ckpt_perror("ckpt_restore_policy", 0);
			ckpt_free(segobj);
			return (-1);
		}
		/*
		 * Read memory image before setting attributes
		 */
		if (ckpt_read_image(tp, &memmap, segobj, nsegs) < 0) {
			ckpt_perror("ckpt_read_image", 0);
			ckpt_free(segobj);
			return (-1);
		}
		if (ckpt_restore_attrs(
				&tp->memobj,
				&memmap,
				segobj,
				nsegs,
				crp->cr_pci.pci_flags & CKPT_PCI_UNCACHEABLE,
				tp->pi.cp_psi.ps_lock) < 0) {
			ckpt_perror("ckpt_restore_attrs", 0);
			ckpt_free(segobj);
			return (-1);
		}
		ckpt_free(segobj);
	}
	return (0);
}

/*
 * Restore the memory image of he process.
 *
 * It would be nice to carry the "object" concept completely through and
 * read the CKPT_MEMMAP objects from the statefile using the property
 * rotines.  However, due to the fact that we're running in a restricted memory
 * space, managing the process property table presents some issues.  It
 * can be arbitrarily large, and anywhere we place it might conflict with
 * the targets memory.
 *
 * So, the MEMMAP objects and their subordinate segment objects and aux data
 * objects are placed sequentially behind the memobj and we use this knowledge
 * throughout
 */
static void
ckpt_restore_image(ckpt_ta_t *tp)
{
	char mempool[MAXDATASIZE]; /* for things needed to be in low mem */
	ckpt_phandle_t *cpr_share_prop_table_low;
	ckpt_phandle_t *cpr_proc_prop_table_low;
	long used = 0, size;
	int brk_restored = 0;
	int depend_sync = 0;
	int prattach = 0;
	int errfd = tp->errfd;
	int errno;
	int cprfd = tp->cprfd;
	pid_t mypid = get_mypid();
	ckpt_ta_t target_low;
	struct rlimit rlim[RLIM_NLIMITS];
	struct itimerval itimer[3];
	sigaction_t sap[MAXSIG];
	int memfd;
	int doshared = 1;
	extern struct ckpt_prop_handle cpr_share_prop_table;
	extern struct ckpt_prop_handle cpr_proc_prop_table;

#if defined(DEBUG) || defined (DEBUG_READV)
	debug_pid = mypid;
#endif
	/*
	 * Gross hack to avoid having to pass errfd and mypid all over the
	 * place.  Also set up thread private errno.
	 * Shared addr sprocs need a unique location to store the
	 * error fd.  The stack is MAP_LOCAL, so can be used for this.  All
	 * share members will store errfd at same vaddr on stack, but MAP_LOCAL
	 * makes each a unique physical location.  And if we're not
	 * a shared address spoc, this works fine too.
	 */
	errfd_p = &errfd;
	errpid_p = &mypid;
	/*
	 * copy the targargs and cr_t to the low memory and refill the prop index
	 */
	bcopy(tp, &target_low, sizeof (ckpt_ta_t));
	bcopy(&cr, &cr_low, sizeof (cr_t));
	bcopy(tp->pi.cp_psi.ps_rlimit, rlim, sizeof(rlim));
	bcopy(tp->pi.cp_itimer, itimer, sizeof(itimer));
	bcopy(tp->sap, sap, tp->sighdr.cs_nsigs * sizeof(sigaction_t));
	target_low.sap = sap;
	target_low.crp = &cr_low;

	size = (long)(sizeof(ckpt_prop_t) * cr_low.cr_pci.pci_prop_count);
	if ((cr_low.cr_index = (ckpt_prop_t *)ckpt_alloc_mempool(
		size, mempool, &used)) == NULL) {
		ckpt_perror("Failed to allocate memory", ERRNO);
		ckpt_abort(cprfd, mypid, target_low.pindex);
	}
	bcopy(cr.cr_index, cr_low.cr_index, size);

	/*
	 * Copy the table down for the low mem objects
	 */
	size = (long)(cr.cr_handle_count * sizeof (ckpt_phandle_t));
	if ((cpr_share_prop_table_low = (ckpt_phandle_t *)ckpt_alloc_mempool(
		size, mempool, &used)) == NULL) {
		ckpt_perror("Failed to allocate memory", ERRNO);
		ckpt_abort(cprfd, mypid, target_low.pindex);
	}
	bcopy(&cpr_share_prop_table, cpr_share_prop_table_low, size);
	target_low.share_prop_tab = cpr_share_prop_table_low;

	size = (long)(cr.cr_procprop_count * sizeof (ckpt_phandle_t));
	if ((cpr_proc_prop_table_low = (ckpt_phandle_t *)ckpt_alloc_mempool(
		size, mempool, &used)) == NULL) {
		ckpt_perror("Failed to allocate memory", ERRNO);
		ckpt_abort(cprfd, mypid, target_low.pindex);
	}
	bcopy(&cpr_proc_prop_table, cpr_proc_prop_table_low, size);
	target_low.proc_prop_tab = cpr_proc_prop_table_low;
	/*
	 * Read in the memobj while the property index pointed to by target
	 * args is stii valid...i.e., before relvm below
	 */
	if (ckpt_read_proc_property_low(&target_low, CKPT_MAGIC_MEMOBJ) < 0) {
		ckpt_abort(cprfd, mypid, target_low.pindex);
	}
	/*
	 * POINT OF NO RETURN!!!
	 * ckpt_relvm unmaps almost everything except the reserved area.
	 *
	 * This has implications for PR_SADDR sprocs.  That is, they all
	 * need to have made into this library before the vm is released.
	 * So, the last sproc through here should release the (shared) address
	 * space.  (The others release prda, if it exists...it may due to
	 * rld mp handling.)  The others cannot really begin mapping because
	 * relvm will undo their work (excluding MAP_LOCAL mappings)
	 *
	 * For now, if not going to do release, block.  The sproc that
	 * releases will unblock all its buddies.  We can be smarter about how 
	 * far we let othrs go before blocking them.
	 *
	 * The doshared flag controls whether this proc does release.  It's
	 * on by default and gets turned off if we're not supposed to do it.
	 */
	if (target_low.pi.cp_psi.ps_shaddr && 
		(target_low.pi.cp_psi.ps_shmask & PR_SADDR)) {

		doshared = ckpt_sproc_block(	mypid,
						saddrlist->saddr_pid,
						saddrlist->saddr_count,
						&saddrlist->saddr_sync);
		if (doshared < 0) {
			ckpt_perror("ckpt_sproc_block", ERRNO);
			ckpt_abort(cprfd, mypid, target_low.pindex);
		}
	}
	/*
	 * It's safe to update this global now, since all shared addrs have checked in
	 */
	__errnoaddr = &errno;

	if (ckpt_relvm(&target_low, target_low.procpath, doshared,
				target_low.pi.cp_psi.ps_lock) < 0)
		ckpt_abort(cprfd, mypid, target_low.pindex);

	if (target_low.pi.cp_psi.ps_shaddr && doshared && 
		(target_low.pi.cp_psi.ps_shmask & PR_SADDR)) {
		/*
		 * let my people go!
		 */
		if (ckpt_sproc_unblock(	mypid,
					saddrlist->saddr_count,
					saddrlist->saddr_pid) < 0) {
			ckpt_perror("ckpt_sproc_unblock", 0);
			ckpt_abort(cprfd, mypid, target_low.pindex);
		}
	}
	if (ckpt_restore_mappings(&target_low, 
				  mypid,
				  0,			/* tid */
				  0,			/* local only? */
				  &depend_sync,
				  &prattach,
				  &brk_restored) < 0) {
		ckpt_abort(cprfd, mypid, target_low.pindex);
	}
	if (!brk_restored && target_low.memobj.cmo_brkbase) {
		setbrk(target_low.memobj.cmo_brkbase);

		if (syssgi(SGI_CKPT_SYS, CKPT_SETBRK, target_low.memobj.cmo_brkbase) < 0) {
			ckpt_perror("CKPT_SETBRK", ERRNO);
			ckpt_abort(cprfd, mypid, target_low.pindex);
		}
	}
	if (!depend_sync) {
		depend_sync = 1;
		if (ckpt_read_share_property_low(&target_low,
						 CKPT_MAGIC_DEPEND, 
						 (void *)&mypid) < 0)
			ckpt_abort(cprfd, mypid, target_low.pindex);
	}
	if (!prattach) {
		if (ckpt_read_share_property_low(&target_low,
						 CKPT_MAGIC_PRATTACH, 
						 (void *)&mypid) < 0)
			ckpt_abort(cprfd, mypid, target_low.pindex);
	}
	/*
	 * shared address sprocs need to sync up so all have address
	 * space intact before cpr can finish restoring state
	 */
	if (target_low.pi.cp_psi.ps_shaddr && 
		(target_low.pi.cp_psi.ps_shmask & PR_SADDR)) {
		int dounblock;

		dounblock = ckpt_sproc_block(	mypid,
						saddrlist->saddr_pid,
						saddrlist->saddr_count,
						&saddrlist->saddr_sync);
		if (dounblock < 0) {
			ckpt_perror("ckpt_sproc_block", ERRNO);
			ckpt_abort(cprfd, mypid, target_low.pindex);
		}
		if (dounblock) {
			if (ckpt_sproc_unblock(	mypid,
						saddrlist->saddr_count,
						saddrlist->saddr_pid) < 0) {
				ckpt_perror("ckpt_sproc_unblock", 0);
				ckpt_abort(cprfd, mypid, target_low.pindex);
			}
		}
	}
	/*
	 * Restore pthreads
	 */
	if (ckpt_init_proc_property(&target_low) < 0)
		ckpt_abort(cprfd, mypid, target_low.pindex);

	if (ckpt_read_proc_property_low(&target_low, CKPT_MAGIC_THREAD) < 0)
		ckpt_abort(cprfd, mypid, target_low.pindex);

	ckpt_pthread_wait();

	ckpt_free_proc_property(&target_low);
	/*
	 * Restore unnamed Posix semaphores
	 */
	if (ckpt_read_share_property_low(&target_low,
					CKPT_MAGIC_PUSEMA, 
					(void *)&mypid) < 0)
			ckpt_abort(cprfd, mypid, target_low.pindex);

	/* for pages in the shared segments with first touch/default policy, 
	 * migrate the pages to where they originaly belong to.
	 */
	if (ckpt_read_share_property_low(&target_low,
					CKPT_MAGIC_PMI,
					(void *)&mypid) < 0)
			ckpt_abort(cprfd, mypid, target_low.pindex);
	/*
	 * close statefiles
	 */
	(void)close(target_low.sifd);
	(void)close(target_low.ssfd);
	(void)close(target_low.mfd);
	/*
	 * Every process and sproc needs to reset its vnode of executable again
	 */
	if (target_low.pi.cp_stat == SZOMB)
		memfd = -1;
	else {
		assert(execpath[0]);
		if ((memfd = ckpt_open(&target_low, execpath, O_RDONLY, 0)) < 0) { 
			ckpt_perror("execpath open", ERRNO);
			ckpt_abort(cprfd, mypid, target_low.pindex);
		}
	}
	if (syssgi(SGI_CKPT_SYS, CKPT_SETEXEC, memfd) < 0) {
		if (memfd >= 0) close(memfd);
		ckpt_perror("CKPT_SETEXEC", ERRNO);
		ckpt_abort(cprfd, mypid, target_low.pindex);
	}
	if (memfd >= 0) close(memfd);
	/*
	 * switch contexts to users and away we go!
	 */
	ckpt_restore_context(cprfd, mypid, &target_low, rlim, itimer);
	/* no return */
	ckpt_perror("client return", 0);
	ckpt_abort(cprfd, mypid, target_low.pindex);
}

