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
#ident	"$Revision: 1.32 $"

#include <sys/types.h>
#include <ksys/as.h>
#include <sys/alenlist.h>
#include <sys/cachectl.h>
#include <sys/capability.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <ksys/fdt.h>
#include <sys/mman.h>
#include <sys/lock.h>
#include <os/proc/pproc_private.h>
#include <sys/reg.h>
#include <sys/sat.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <ksys/vfile.h>
#include <sys/vnode.h>
#include <sys/dmi.h>
#include <sys/dmi_kern.h>
#include <sys/cred.h>
#include <ksys/vproc.h>
#include <ksys/vm_pool.h>
#include <ksys/vpag.h>

/*
 * System calls relating to Address Space manipulation
 * Also - 'helper' functions that kernel modules can call that
 * translate into VAS ops
 */
extern int vce_avoidance;

/*
 * isolatereg(ut, cpu)
 * called everytime a thread wants to migrate to an isolated processor
 * ut can be different from curuthread
 */
void
isolatereg(uthread_t *ut, cpuid_t cpu)
{
	vasid_t vasid;
	as_setattr_t asattr;

	ut_flagset(ut, UT_ISOLATE);

	/*
	 * if we can't get a ref then there is no address and nothing to
	 * do!
	 */
	if (as_lookup(ut->ut_asid, &vasid))
		return;

	asattr.as_op = AS_SET_ISOLATE;
	asattr.as_iso_cpu = cpu;
	VAS_SETATTR(vasid, NULL, &asattr);
	as_rele(vasid);
}

/*
 * unisolatereg(ut, cpu)
 * called everytime a thread wants to migrate away from an isolated cpu
 * or the cpu becomes unisolated underneath it.
 * the ut can be different from curuthread
 *
 * If cpu is set to PDA_RUNANYWHERE then unisolating only the thread.
 * If cpu is not set to PDA_RUNANYWHERE, then unisolating the cpu out from
 * under the thread.
 */
void
unisolatereg(uthread_t *ut, cpuid_t cpu)
{
	vasid_t vasid;
	as_setattr_t asattr;

	/*
	 * if we can't get a ref then there is no address and nothing to
	 * do!
	 */
	if (as_lookup(ut->ut_asid, &vasid))
		return;
	asattr.as_op = AS_SET_UNISOLATE;
	asattr.as_iso_cpu = cpu;
	VAS_SETATTR(vasid, NULL, &asattr);
	as_rele(vasid);

	ut_flagclr(ut, UT_ISOLATE);
}

#if _MIPS_SIM == _ABI64
int
chk_kuseg_abi(int abi, uvaddr_t addr, size_t len)
{
	if (!(IS_KUSEG(addr)))
		return EINVAL;

	if (ABI_IS_64BIT(abi)) {
		if (!IS_KUSEG64(addr + MIN(len, KUSIZE_64) - 1))
			return EINVAL;
	} else {
		if (!IS_KUSEG32(addr + MIN(len, KUSIZE_32) - 1))
			return EINVAL;
	}
	return 0;
}
#else
/*ARGSUSED*/
int
chk_kuseg_abi(int abi, uvaddr_t addr, size_t len)
{
	if (!(IS_KUSEG(addr)))
		return EINVAL;

	if (!IS_KUSEG32(addr + MIN(len, KUSIZE_32) - 1))
		return EINVAL;
	return 0;
}
#endif

/*
 * On IP26 (Teton) and IP28 (pacecar) systems with ecc memory, we can't do
 * uncached writes to main memory in normal operation.  So, don't allow
 * the UNCACHED operation to succeed here.
 */
#if _NO_UNCACHED_MEM_WAR
extern int ip26_allow_ucmem;
#endif

/*
 * cachectl() -- cachectl(addr, bcnt, op) system call routine
 */
struct cachectla {
	caddr_t	addr;
	sysarg_t bcnt;
	sysarg_t op;
};

int
cachectl(struct cachectla *uap)
{
	uvaddr_t addr = uap->addr;
	size_t bcnt = uap->bcnt;
	int op = uap->op;
	int error;
	vasid_t vasid;
	as_setrangeattr_t asw;

#if _NO_UNCACHED_MEM_WAR /* IP26 and IP28 baseboards do not allow uncached */
	if (!ip26_allow_ucmem && op == UNCACHEABLE) {
		return EINVAL;
	}
#endif
	/*
	 * Addr must be page aligned, and bcnt must be a pagesize
	 * multiple.
	 */
	if (poff(addr) || poff(bcnt) ||
	    op != CACHEABLE && op != UNCACHEABLE) {
		return EINVAL;
	}
#ifdef MH_R10000_SPECULATION_WAR
        if (IS_R10000() && op == UNCACHEABLE) {
                /* force all pages in and migrate to DMA space */
                if (useracc(addr, bcnt, B_WRITE|B_PHYS, NULL) == 0)
                        unuseracc(addr, bcnt, B_WRITE|B_PHYS);
        }
#endif /* MH_R10000_SPECULATION_WAR */

	as_lookup_current(&vasid);
	asw.as_op = AS_CACHECTL;
	asw.as_cachectl_flags = op;
	error = VAS_SETRANGEATTR(vasid, addr, bcnt, &asw, NULL);

	return error;
}

extern int icache_size, dcache_size;

#ifdef _R5000_JUMP_WAR
extern int dynamic_jump_war_check(caddr_t,uint);
extern int R5000_jump_war_correct;

#define DYNAMIC_JUMP_WAR_CHECK(t,addr,bcnt) \
	((R5000_jump_war_correct && (t)) \
	 ? dynamic_jump_war_check(addr,bcnt) : 0)
#else /* _R5000_JUMP_WAR */
#define DYNAMIC_JUMP_WAR_CHECK(t,addr,bcnt)		((void)0)
#endif /* _R5000_JUMP_WAR */

/*
 * cacheflush() -- cacheflush(addr, bcnt, cache) system call routine
 */
struct cacheflusha {
	caddr_t	addr;
	sysarg_t bcnt;
	sysarg_t cache;
};

int
cacheflush(struct cacheflusha *uap)
{
	/* Make sure that address range is in user space */
	if (!IS_KUSEG((__psunsigned_t)uap->addr + uap->bcnt - 1))
		return EFAULT;

	/* Make sure that bcnt isn't bogus */
	if (uap->bcnt < 0)
		return EFAULT;

	switch (uap->cache) {
	case ICACHE:
		DYNAMIC_JUMP_WAR_CHECK(1, uap->addr, uap->bcnt);
		/* Make sure flush works for modified code */
		cache_operation(uap->addr, uap->bcnt,
				CACH_ICACHE_COHERENCY |
				CACH_DCACHE | CACH_WBACK | CACH_VADDR);
		cache_operation(uap->addr, uap->bcnt,
				CACH_FORCE |
				CACH_ICACHE | CACH_INVAL | CACH_VADDR);
		break;
	case DCACHE:
		cache_operation(uap->addr, uap->bcnt,
				CACH_FORCE |
				CACH_DCACHE | CACH_INVAL |
				CACH_WBACK | CACH_VADDR);
		break;
	case BCACHE:
		DYNAMIC_JUMP_WAR_CHECK(1, uap->addr, uap->bcnt);
		cache_operation(uap->addr, uap->bcnt,
				CACH_FORCE |
				CACH_DCACHE | CACH_INVAL |
				CACH_WBACK | CACH_VADDR);
		cache_operation(uap->addr, uap->bcnt,
				CACH_FORCE |
				CACH_ICACHE | CACH_INVAL | CACH_VADDR);
		break;
	default:
		return EINVAL;
	}

	return 0;
}

#if SW_FAST_CACHE_SYNCH

/*
 * gets called when user issues break BRK_CACHE_SYNC
 * instruction to synchronize dache and icaches. If
 * bcnt is small and addr is present in tlb, some
 * synchronization is performed in locore.s. If not
 * trap() calls this routine to complete the work.
 * Also called from syssgi(SGI_SYNCH_CACHE_HANDLER
 * to synchronize caches for handler code that it
 * returns to library calls that asks for cache synch
 * handler.
 */
int
cachesynch(caddr_t addr, uint bcnt)
{
        struct cacheflusha ua;

        ua.addr = addr;
        ua.bcnt = bcnt;
        ua.cache = BCACHE;

        return cacheflush(&ua);
}
#endif /* SW_FAST_CACHE_SYNCH */

struct locka {
	sysarg_t	oper;
};

int
lock(struct locka *uap)
{
	int attr;
	as_setrangeattrop_t op;
	vasid_t vasid;
	as_setrangeattr_t asattr;

	if (!_CAP_ABLE(CAP_MEMORY_MGT))
		return EPERM;

	switch (uap->oper) {
	case TXTLOCK:
		op = AS_LOCK_BY_ATTR;
		attr = AS_TEXTLOCK;
		break;

	case PROCLOCK:
		op = AS_LOCK_BY_ATTR;
		attr = AS_TEXTLOCK|AS_DATALOCK|AS_PROCLOCK;
		break;

	case DATLOCK:
		op = AS_LOCK_BY_ATTR;
		attr = AS_DATALOCK;
		break;

	case UNLOCK:
		op = AS_UNLOCK_BY_ATTR;
		attr = AS_TEXTLOCK|AS_DATALOCK|AS_PROCLOCK;
		break;

	default:
		return EINVAL;
	}

	/*
	 * Batch processes cannot pin memory. So suspend them until they
  	 * go critical. If we let them lock pages and they later on wait
	 * for ever to go critical, it is possible that we locked a piece 
	 * memory for a very long time.
 	 */
	as_lookup_current(&vasid);
	while (kt_basepri(curthreadp) == PBATCH) {
		as_getasattr_t as_vpag_attr;
		VAS_GETASATTR(vasid, AS_VPAGG, &as_vpag_attr);
		if (as_vpag_attr.as_vpagg)
			VPAG_SUSPEND(as_vpag_attr.as_vpagg);
	}
		

	asattr.as_op = op;
	asattr.as_lock_attr = attr;
	return VAS_SETRANGEATTR(vasid, 0, 0, &asattr, NULL);
}

/*
 * Used for the following system calls:
 * mpin(), munpin(), mlock(), munlock(), mlockall() and munlockall()
 */
struct pagelocka {
	char		*addr;
	sysarg_t	len;
	sysarg_t	inout;
};

int
pagelock(struct pagelocka *uap)
{
	int error;
	vasid_t vasid;
	int attr;
	as_setrangeattrop_t op;
	as_setrangeattr_t asattr;

	switch (uap->inout) {
	case PGLOCK:
		op = AS_LOCK;
		break;
	case UNLOCK:
		op = AS_UNLOCK;
		break;
	case PGLOCKALL:
		op = AS_LOCKALL;
		break;
	case UNLOCKALL:
		op = AS_UNLOCKALL;
		break;
	case FUTURELOCK:
		op = AS_LOCK_BY_ATTR;
		attr = AS_FUTURELOCK;
		break;
	default:
		return EINVAL;
	}

	as_lookup_current(&vasid);

	/*
	 * Batch processes cannot pin memory.
 	 */
	while (kt_basepri(curthreadp) == PBATCH) {
		as_getasattr_t as_vpag_attr;
		VAS_GETASATTR(vasid, AS_VPAGG, &as_vpag_attr);
		if (as_vpag_attr.as_vpagg)
			VPAG_SUSPEND(as_vpag_attr.as_vpagg);
	}

	asattr.as_op = op;
	asattr.as_lock_attr = attr;
	error = VAS_SETRANGEATTR(vasid, uap->addr, uap->len, &asattr, NULL);

	if (error == EAGAIN)
		nomemmsg("mpin");

	return error;
}

/*
 * Kernel versions of mpin/munpin to be used by graphics drivers.
 */

int
kern_mpin(caddr_t vaddr, size_t len)
{
	struct	pagelocka params;

	params.addr = vaddr;
	params.len = len;
	params.inout =  PGLOCK;
	return (pagelock(&params));
}

int
kern_munpin(caddr_t vaddr, size_t len)
{
	struct	pagelocka params;

	params.addr = vaddr;
	params.len = len;
	params.inout =  UNLOCK;
	return (pagelock(&params));
}

/*
 * lock down (and allocate if necessary) prda
 */
int
lockprda(struct prda **prda)
{
	as_addspace_t asd;
	as_addspaceres_t asres;
	vasid_t vasid;
	int error;

	as_lookup_current(&vasid);

	asd.as_op = AS_ADD_PRDA;
	if (error = VAS_ADDSPACE(vasid, &asd, &asres))
		return error;
	*prda = (struct prda *)asres.as_addr;
	ASSERT(*prda);
	return 0;
}

/* 
 * removetsave - calls to remove any saved pregions
 * pregions that are pure text are saved during exec..
 */
void
removetsave(int flags)
{
	vasid_t vasid;
	as_shake_t shake;

	as_lookup_current(&vasid);
	shake.as_shaketsave_flags = flags;
	(void) VAS_SHAKE(vasid, AS_SHAKETSAVE, &shake);
}

/*
 * grow the stack to include the SP (Stacks grow down!)
 *
 * Returns: 0 on success or one of the following errors in case of failure:
 *
 *	    EEXIST  if sp already valid
 *	    ENOSPC  if couldn't grow cause process is too large
 *	    EACCES  if don't have no stack
 *	    ENOMEM  couldn't lock down stack pages
 *	    EAGAIN  if couldn't grow for other reasons
 */
int
grow(uvaddr_t sp)
{
	int error;
	vasid_t vasid;

	as_lookup_current(&vasid);
retry_stack :
	error = VAS_GROW(vasid, AS_GROWSTACK, sp);
	if (error == EMEMRETRY) {
		if (vm_pool_wait(GLOBAL_POOL)) {
			goto retry_stack;
		} else error = EAGAIN;
	}

	if (error == EAGAIN) 
		nomemmsg("stack growth");
		
	return error;
}

/*
 * brk and sbrk system calls
 */
struct brka {
	sysarg_t nva;
};

int
sbreak(struct brka *uap)
{
	int error;
	vasid_t vasid;

	as_lookup_current(&vasid);
retry_sbreak:
	error = VAS_GROW(vasid, AS_GROWBRK, (uvaddr_t)uap->nva);

	if (error == EMEMRETRY) {
		if (vm_pool_wait(GLOBAL_POOL)) {
			goto retry_sbreak;
		} else error = EAGAIN;
	} 

	if (error == EAGAIN) {
		nomemmsg("brk/sbrk");
	}

	return error;
}

/* ARGSUSED */
int
getpagesize(void *uap, rval_t *rvp)
{
	rvp->r_val1 = NBPC;
	return 0;
}

struct mmap_comm_args {
	caddr_t		addr;
	long		len;
	long		prot;
	long		flags;
	long		fd;
	off_t		off;
};

static int mmap_common(struct mmap_comm_args *, rval_t *);

/*
 * mmap64 system call
 */
struct irix5_mmap64a {
	caddr_t		addr;
	sysarg_t	len;
	sysarg_t	prot;
	sysarg_t	flags;
	sysarg_t	fd;
	sysarg_t	pad;	/* off is 8-byte aligned in user's stack */
	sysarg_t	off1;
	sysarg_t	off2;
};

/*
 * n32 and n64 binaries are directed to mmap by libc.  But since
 * o32 and n32 kernels do not save 64 bit arg registers, n32 mmaps are
 * sent back here by mmap.
 */
int
mmap64(struct irix5_mmap64a *uap, rval_t *rvp)
{
	struct mmap_comm_args	args;
	int error;

	ASSERT(ABI_IS(get_current_abi(), (ABI_IRIX5|ABI_IRIX5_N32)));

	uap = (struct irix5_mmap64a *)uap;
	args.addr = uap->addr;
	args.len = uap->len;
	args.prot = uap->prot;
	args.flags = uap->flags;
	args.fd = uap->fd;
	args.off = (((off_t)(uap->off1) & 0xffffffff) << 32) |
			(uap->off2 & 0xffffffff);
	error = mmap_common(&args, rvp);
	_SAT_FD_READ(((struct mmap_comm_args *)uap)->fd,error);
	return error;
}

/*
 * mmap system call
 */
struct mmapa {
	caddr_t		addr;
	sysarg_t	len;
	sysarg_t	prot;
	sysarg_t	flags;
	sysarg_t	fd;
	sysarg_t	off;
};

int
mmap(struct mmapa *uap, rval_t *rvp)
{
	struct mmap_comm_args args;
	int error;

#if (_MIPS_SIM == _ABIO32) || (_MIPS_SIM == _ABIN32)
	if (ABI_IS_IRIX5_N32(get_current_abi()))
		return(mmap64((struct irix5_mmap64a *) uap, rvp));
#endif

	args.addr = uap->addr;
	args.len = uap->len;
	args.prot = uap->prot;
	args.flags = uap->flags;
	args.fd = uap->fd;
	args.off = (off_t)uap->off;

	error = mmap_common(&args, rvp);
	_SAT_FD_READ(uap->fd,error);
	return error;
}

static int
mmap_common(struct mmap_comm_args *uap, rval_t *rvp)
{
	struct vfile		*fp;
	vnode_t			*vp, *oldvp;
	auto uvaddr_t		vaddr;
	size_t			len;
	off_t			off;
	int			type;
	int			error;
	int			isspec = 0;
	mprot_t			prot;
	mprot_t			maxprot;
	vasid_t			vasid;
	as_addspace_t		asadd;
	as_addspaceres_t	asres;
	extern dev_t		zeroesdev;
	dm_fcntl_t		dmfcntl;

	/* Offset must be a multiple of page size */
	off = uap->off;
	if (off & POFFMASK)
		return EINVAL;
	
	/*
	 * protect against vaddr + xxx wrapping
	 */
	vaddr = uap->addr;
	if (vaddr && !IS_KUSEG(vaddr))
		return EINVAL;

	/* Don't let user pass extraneous bits */
	prot = (mprot_t)uap->prot;

	if ((prot & PROT_ALL) != prot)
		return EACCES;

	/*
	 * We can't do write-only or execute-only access.  So if you ask
	 * for PROT_WRITE or PROT_EXEC, you get PROT_READ as well.
	 */
	if (prot & (PROT_WRITE | PROT_EXEC))
		prot |= PROT_READ;

	/*
	 * A little sanity checking on length.
	 * We don't check len == 0 for non-IFREG mappings?
	 * Basically, so that off+len doesn't wrap we take advantage of
	 * the fact that one is only permitted to use 1/2 the address space
	 */
	len = uap->len;
	if ((ssize_t)len < 0)
		return ENXIO;

	type = (uap->flags & MAP_TYPE);
	if (type != MAP_SHARED && type != MAP_PRIVATE)
		return EINVAL;

	/* Does fd reference an open file descriptor? */
	if (error = getf((int)uap->fd, &fp))
		return error;

	if (!VF_IS_VNODE(fp))
		return EINVAL;
	vp = VF_TO_VNODE(fp);

	maxprot = PROT_ALL;	/* start off allowing all access */

	/*
	 * Check if file is writable for shared mappings.
	 * Only writes of shared mappings actually cause writes to the fd.
	 * Private mappings can always be written.
	 */
	if ((type == MAP_SHARED) && ((fp->vf_flag & FWRITE) == 0)) {
		/* no write access allowed */
		maxprot &= ~PROT_WRITE;
	}

	/*
	 * Verify that requested prots aren't greater than maxprots.  We
	 * must also be able to read the file since write-only mappings
	 * cannot be implemented.
	 */
	if ((maxprot & prot) != prot || (fp->vf_flag & FREAD) == 0)
		return EACCES;

	if (vp->v_type == VCHR || vp->v_type == VBLK) {
		isspec = 1;
		if ((vp->v_rdev == zeroesdev) && (len == 0))
			return(ENXIO);	/* do the work of zeromap */
	} else {

		/* non-spec files - constrain 'off' to be positive
		 * this helps assure that off+len doesn't wrap
		 */
		if ((__scint_t)off < 0 || (__scint_t)(off + len) < 0)
			return EINVAL;

		if (len == 0)
			return ENXIO;
		if (vp->v_type != VREG)
			return(ENODEV);

	}

	if (!(uap->flags & MAP_FIXED) && vaddr != NULL) {
		/*
		 * The address given is a "hint".  Round it up to a 
		 * page boundary and give it a try.  If it fails, we'll
		 * allocate an address and try one more time.
		 */
		vaddr = (caddr_t) ctob(btoc(vaddr));
#ifdef _VCE_AVOIDANCE
		if (vce_avoidance) {
			if (!isspec) {
				int	ucl;
				
				ucl = colorof(off) - colorof(vaddr);
				if (ucl < 0)
					ucl += (cachecolormask + 1);
				vaddr += ctob(ucl);
			}
		}
#endif
	}

#ifndef CELL_IRIX
	/*
	 * Generate DMAPI managed region events for the whole file if
	 * needed.  
	 *
	 * Note that the VOP_FCNTL used here returns an error if the
	 * underlying file system is unaware of the F_DMAPI subfunction
	 * being used.  In that case this call is just a no-op and no harm
	 * is done by the VOP_FCNTL call.
	 */
	dmfcntl.dmfc_subfunc = DM_FCNTL_MAPEVENT;
	dmfcntl.u_fcntl.maprq.length = len;
	dmfcntl.u_fcntl.maprq.max_event =
			(maxprot & PROT_WRITE) && (type == MAP_SHARED) ?
				DM_EVENT_WRITE : DM_EVENT_READ;

	VOP_FCNTL(vp, F_DMAPI, &dmfcntl, 0, off, sys_cred, NULL, error);
	if (error == 0) {
		if ((error = dmfcntl.u_fcntl.maprq.error) != 0)
			return error;
	}
#endif

	/*
	 * check if ok with file system to be mapped
	 * We could get back a different vp ... (lofs)
	 *
	 * If a new vp is returned, then it has a reference
	 * that we must VN_RELE.
	 */
	oldvp = vp;
	VOP_MAP(vp, off, len, prot, uap->flags, fp->vf_cred, &vp, error);
	if (error)
		return error;

	as_lookup_current(&vasid);
		
	asadd.as_op = isspec ? AS_ADD_MMAPDEV : AS_ADD_MMAP;
	asadd.as_addr = vaddr;
	asadd.as_length = len;
	asadd.as_prot = prot;
	asadd.as_maxprot = maxprot;
	asadd.as_mmap_off = off;
	asadd.as_mmap_vp = vp;
	asadd.as_mmap_flags = uap->flags;
#if CKPT
	asadd.as_mmap_ckpt = fp->vf_ckpt;
#endif
	if (error = VAS_ADDSPACE(vasid, &asadd, &asres)) {
		if (error == EAGAIN)
			nomemmsg("mmap");
		goto out;
	}

	rvp->r_val1 = (__psint_t)asres.as_addr;

 out:	
	if (oldvp != vp)
		VN_RELE(vp);		/* release ref. from VOP_MAP */

	return error;
}

/*
 *	munmap system call
 */
struct munmapa {
	caddr_t addr;
	sysarg_t len;
};

int
munmap(struct munmapa *uap)
{
	vasid_t	vasid;
	as_deletespace_t asd;

	if (uap->len <= 0)
		return EINVAL;
	if (poff(uap->addr))
		return EINVAL;
	if (chk_kuseg_abi(get_current_abi(), uap->addr, uap->len))
		return EINVAL;

	as_lookup_current(&vasid);
	asd.as_op = AS_DEL_MUNMAP;
	asd.as_munmap_start = uap->addr;
	asd.as_munmap_len = uap->len;
	asd.as_munmap_flags = 0;
	return VAS_DELETESPACE(vasid, &asd, NULL);
}

/*
 *	msync system call
 */
struct msynca {
	caddr_t		addr;
	sysarg_t	len;
	sysarg_t	flags;
};

int
msync(struct msynca *uap)
{
	uvaddr_t vaddr;
	size_t len;
	vasid_t	vasid;
	as_setrangeattr_t attr;

	vaddr = uap->addr;
	len = uap->len;

	if (poff(vaddr) != 0)
		return EINVAL;

	if (chk_kuseg_abi(get_current_abi(), vaddr, len))
		return ENOMEM;

	as_lookup_current(&vasid);

	attr.as_sync_flags = uap->flags;
	attr.as_op = AS_SYNC;
	return VAS_SETRANGEATTR(vasid, vaddr, len, &attr, NULL);
}

struct mprotecta {
	caddr_t		addr;
	usysarg_t	len;
	sysarg_t	prot;
};

int
mprotect(struct mprotecta *uap)
{
	uvaddr_t vaddr = uap->addr;
	mprot_t new_prot = uap->prot;
	vasid_t	vasid;
	as_setrangeattr_t attr;

	if (poff(vaddr) != 0)
		return EINVAL;
	if (chk_kuseg_abi(get_current_abi(), vaddr, uap->len))
		return ENOMEM;

	/*
	 * PROT_EXEC_NOFLUSH is really a special case of PROT_EXEC so
	 * make sure the PROT_EXEC flag is set as well
	 */
	if(new_prot & PROT_EXEC_NOFLUSH)
		new_prot |= PROT_EXEC;


	/*
	 * The mips hardware can't support write-only and execute-only
	 * mappings, so people asking for PROT_WRITE or PROT_EXEC get
	 * PROT_READ as well.
	 */
	if (new_prot & (PROT_WRITE|PROT_EXEC))
		new_prot |= PROT_READ;

	as_lookup_current(&vasid);

	attr.as_op = AS_PROT;
	attr.as_prot_prot = new_prot;
	attr.as_prot_flags = 0;
	return VAS_SETRANGEATTR(vasid, vaddr, uap->len, &attr, NULL);
}

struct madvisea {
	caddr_t		addr;
	sysarg_t	len;
	sysarg_t	behav;
};

int
madvise(struct madvisea *uap)
{
	vasid_t	vasid;
	uvaddr_t vaddr = uap->addr;
	size_t len = uap->len;
	as_setrangeattr_t attr;

	as_lookup_current(&vasid);

	/*
	 * Page align vaddr if it isn't,
	 * and add the difference to length.
	 */
	len += poff(vaddr);
	vaddr = (uvaddr_t)pbase(vaddr);

	if (poff(vaddr) != 0)
		return EINVAL;
	if (chk_kuseg_abi(get_current_abi(), vaddr, len))
		return ENOMEM;

	attr.as_op = AS_ADVISE;
	attr.as_advise_bhv = uap->behav;
	return VAS_SETRANGEATTR(vasid, vaddr, len, &attr, NULL);
}

/*
 *
 * TRAP interfaces - tlbmiss/tlbmod
 */
int
vfault(uvaddr_t vaddr, int rw, eframe_t *ep, int *code)
{
	as_fault_t as;
	as_faultres_t asres;
	vasid_t vasid;
	int issc = 0, error;

	as_lookup_current(&vasid);

	as.as_epc = (uvaddr_t)ep->ef_epc;
	as.as_uvaddr = vaddr;
	as.as_ut = curuthread;
	as.as_rw = rw;
	as.as_flags = 0;
	as.as_flags |= USERMODE(ep->ef_sr) ? ASF_FROMUSER : 0;
	as.as_flags |= PTRACED(curprocp) ? ASF_PTRACED : 0;
	if (as.as_flags & ASF_PTRACED) {
		/* can only have interesting watchpoints if being traced */
		as.as_szmem = sizememaccess(ep, &issc);
		as.as_flags |= issc ? ASF_ISSC : 0;
		ASSERT(as.as_szmem != 0);
	}

	/*
	 * try to catch bogus vfaults - note that sizememaccess takes
	 * into account BADVA WARs
	 */
	ASSERT((__psunsigned_t)vaddr < (__psunsigned_t)K0SEG);

#if FAST_LOCORE_TFAULT
	if (curvprocp == NULL)
		cmn_err_tag(107,CE_PANIC, "vfault: curvproc is NULL\n");
#endif

	error = VAS_FAULT(vasid, AS_VFAULT, &as, &asres);
	*code = asres.as_code;
	return error;
}

int
pfault(uvaddr_t vaddr, eframe_t *ep, int *code, int rw)
{
	as_fault_t as;
	as_faultres_t asres;
	vasid_t vasid;
	int issc = 0, error;

	as_lookup_current(&vasid);

	as.as_uvaddr = vaddr;
	as.as_ut = curuthread;
	as.as_rw = rw;
	as.as_flags = 0;
	if (ep) {
		/* came from user level exception */
		as.as_epc = (uvaddr_t)ep->ef_epc;
		as.as_flags |= USERMODE(ep->ef_sr) ? ASF_FROMUSER : 0;
		as.as_flags |= PTRACED(curprocp) ? ASF_PTRACED : 0;
		if (as.as_flags & ASF_PTRACED) {
			/* can only have interesting watchpoints if being traced */
			as.as_szmem = sizememaccess(ep, &issc);
			as.as_flags |= issc ? ASF_ISSC : 0;
		}
	} else {
		as.as_epc = AS_ADD_UNKVADDR;
		as.as_flags |= ASF_NOEXC;
	}

	error = VAS_FAULT(vasid, AS_PFAULT, &as, &asres);
	*code = asres.as_code;
	return error;
}

/*
 *	Attach to a user page.
 *	Make a user's page copy-on-write and otherwise nail it down
 *	so that a kernel buffer system can use the page directly.
 *	This feature is used to avoid copying user data into a kernel
 *	buffer.
 *
 *	When do_sync is 0, the caller is telling pattach()
 *	not to take care of synchronizing the tlbs. Before
 *	allowing the flipped pages to be accessed, either
 *	locally or on another CPU, you must call psync(). The more
 *	rarely you call psync(), the better your performance will be.
 *
 */
caddr_t	
pattach(uvaddr_t vaddr, int do_sync)
{
	vasid_t vasid;
	as_share_t as;
	as_shareres_t asres;

	as_lookup_current(&vasid);
	as.as_share_pattach.as_vaddr = vaddr;
	as.as_share_pattach.as_dosync = do_sync;

	if (VAS_SHARE(vasid, AS_PATTACH, &as, &asres))
		return NULL;
	return asres.as_kvaddr;
}

/*
 *      Flip a page to user space.
 *      This is used to avoid copying data from the kernel to the user.
 *      It takes a kernel virtual address and a user virtual address,
 *      and tries to replace the physical memory represent the latter
 *      with the former.
 *
 *	When do_sync is 0, the caller is telling pflip()
 *	not to take care of synchronizing the tlbs. Before
 *	allowing either of the flipped pages to be accessed, either
 *	locally or on another CPU, you must call psync(). The more
 *	rarely you call psync(), the better your performance will be.
 *
 *      On success, the kernel page has become part of user space
 *      and the page that may have been there before has been released.
 *
 *      Return 0 if unable to flip or kernel-mapped version of target
 */
caddr_t	
pflip(caddr_t kva, uvaddr_t tgt, int do_sync)
{
	vasid_t vasid;
	as_share_t as;
	as_shareres_t asres;

	as_lookup_current(&vasid);
	as.as_share_pflip.as_kvaddr = kva;
	as.as_share_pflip.as_tgtaddr = tgt;
	as.as_share_pflip.as_dosync = do_sync;

	if (VAS_SHARE(vasid, AS_PFLIP, &as, &asres))
		return NULL;
	return asres.as_kvaddr;
}

/*
 *	Synchronize the TLBs after using pflip() or pattach().
 *	psync() *must* be called before anything is permitted to access
 *	the flipped or attached data, whether on this CPU or another.
 */
void
psync()
{
	vasid_t vasid;
	as_shareres_t asres;

	as_lookup_current(&vasid);
	VAS_SHARE(vasid, AS_PSYNC, NULL, &asres);
	return;
}


/*
 * Fills in array of pfns given a virtual address in a process.
 * Invalid pages show up as zero entries.
 *
 * WARNING: pfn's returned can ONLY be trusted if
 *	1. page is LOCKED since region is unlocked on exit, or
 *
 * Page table won't be swapped out since either page is locked, or the process
 * is running.
 */
int
vtop(uvaddr_t vaddr, size_t nbytes, pfn_t *ppfn, int step)
{
	return(vtopv(vaddr, nbytes, ppfn, step, 0, 0));
}

int
vtopv(
	uvaddr_t vaddr,
	size_t nbytes,
	pfn_t *ppfn,
	int step,
	int shift,
	int bits)		/* # bytes to incr ppfn each time */
{
	vasid_t vasid;
	as_getrangeattrin_t asattrin;
	int error;

	asattrin.as_vtop.as_ppfn = ppfn;
        asattrin.as_vtop.as_ppgsz = 0;
	asattrin.as_vtop.as_step = step;
	asattrin.as_vtop.as_shift = shift;
	asattrin.as_vtop.as_bits = bits;

	as_lookup_current(&vasid);
	error = VAS_GETRANGEATTR(vasid, vaddr, nbytes, AS_GET_VTOP,
					&asattrin, NULL);
	return !error;
}

/*
 * maputokptbl
 *	Given the user virtual address, and kernel virtual address,
 *	and some specific PTE bits, drop in the mapping from user
 *	virtual address to kernel virtual address. It's assumed
 *	that the pages if any in the user virutal address are 
 *	pinned down. 
 *	Although this is pretty similar to vtopv, it's been made 
 *	a different routine to accomodate the fact that PTEs are
 *	not 32 bit for SN0..
 *	Moreover, most of the users of vtopv do not use the ptebits
 *	field (rather they pass it as 0)
 *
 * 	Return value:
 *		Return the number of "valid" pages mapped. 
 */
int
maputokptbl(
	vasid_t vasid,
	uvaddr_t vaddr,
	size_t 	nbytes,
	caddr_t	kvaddr,
	ulong_t	bits)		
{
	as_getrangeattrin_t asattrin;
	as_getrangeattr_t asattrout;
	int error;

	asattrin.as_pde.as_ppde = kvtokptbl(kvaddr);
	asattrin.as_pde.as_bits = bits;

	error = VAS_GETRANGEATTR(vasid, vaddr, nbytes, AS_GET_KPTE,
					&asattrin, &asattrout);
	return error ? 0 : asattrout.as_vtop_nmapped;
}

/*
 * vaddr_to_pde: 
 * 	      Get the pde information for this vaddr from the page tables.
 */
void
vaddr_to_pde(uvaddr_t vaddr, pde_t *pd_info)
{
	vasid_t vasid;
	as_getrangeattrin_t asattrin;
	int error;
	pde_t pde;

	asattrin.as_pde.as_ppde = &pde;
	asattrin.as_pde.as_bits = 0;

	as_lookup_current(&vasid);
	error = VAS_GETRANGEATTR(vasid, vaddr, NBPP, AS_GET_PDE,
					&asattrin, NULL);
	if (error)
		pg_clrpgi(pd_info);
	else
		*pd_info = pde;
}

/*
 * Convert a User Address/Length pair into a Physical Address/Length List.
 * Every page in the user address range specified must be valid in order
 * to call this routine, so its use should be very limited.  TBD: useracc,
 * or something similar could build an Address/Length List as it goes along.
 */
alenlist_t
uvaddr_to_alenlist(
	alenlist_t alenlist,
	uvaddr_t vaddr,
	size_t nbytes,
	unsigned flags)
{
	vasid_t vasid;
	as_getrangeattrin_t asattrin;
	int error;
	int created_alenlist;

	/* If caller supplied a List, use it.  Otherwise, allocate one. */
	if (alenlist == NULL) {
		alenlist = alenlist_create(0);
		created_alenlist = 1;
	} else {
		alenlist_clear(alenlist);
		created_alenlist = 0;
	}
	asattrin.as_alen.as_alenlist = alenlist;
	asattrin.as_alen.as_flags = flags;

	as_lookup_current(&vasid);
	error = VAS_GETRANGEATTR(vasid, vaddr, nbytes, AS_GET_ALEN,
					&asattrin, NULL);
	if (error) {
		if (created_alenlist)
			alenlist_destroy(alenlist);
		return NULL;
	}
	alenlist_cursor_init(alenlist, 0, NULL);
	return(alenlist);
}
