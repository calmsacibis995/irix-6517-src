/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 *	SGI specific system calls
 */

#ident	"$Revision: 3.522 $"

#include <sys/types.h>
#include "os/as/as_private.h"
#include <sys/arsess.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/extacct.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <ksys/vpag.h>
#include <ksys/vm_pool.h>
#include <ksys/vproc.h>
#include <ksys/sthread.h>
#include <ksys/xthread.h>
#include <ksys/vhost.h>
#include <sys/getpages.h>
#include <sys/immu.h>
#include <sys/invent.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/ksignal.h>
#include <sys/kusharena.h>
#ifdef KV_DEBUG
#include <sys/map.h>
#endif
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/proc.h>
#include <sys/procset.h>
#include <sys/kresource.h>
#include <sys/resource.h>
#include <ksys/vsession.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/syssgi.h>
#include <sys/systm.h>
#include <sys/timers.h>
#include <sys/traplog.h>
#include <sys/tuneable.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <sys/sat.h>
#include <sys/eag.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/edt.h>
#include <sys/pio.h>
#include <elf.h>
#include <sys/callo.h>
#include <sys/calloinfo.h>
#include <sys/kopt.h>
#include <sys/runq.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/profil.h>
#include <sys/var.h>
#include <sys/xlate.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <sys/acl.h>
#include <sys/uuid.h>
#include <sys/grio.h>
#include <string.h>
#include <limits.h>
#include <sys/prctl.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_fsops.h>
#include <sys/fs/xfs_itable.h>
#include <sys/fs/xfs_dfrag.h>
#include <sys/fs/xfs_utils.h>
#ifdef _SHAREII
#include	<sys/shareIIstubs.h>
#endif /* _SHAREII */
#include <sys/hwperftypes.h>
#include <sys/dbacc.h>
#include <sys/atomic_ops.h>
#include <sys/mload.h>
#include <sys/kaio.h>
#include <sys/handle.h>
#include <ksys/as.h>
#include <ksys/exception.h>
#include <sys/dpipe.h>
#include <sys/unc.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */
#include "as/pmap.h"
#include <sys/ipc.h>
#include <sys/rt.h>
#include <ksys/cacheops.h>


#include <sys/vme/vmeio.h>

#include <sys/systeminfo.h>

#include <sys/failover.h>
/*
 * REACT/Pro timestamps
 */
#include <sys/rtmon.h>

#if CELL
#include <ksys/cell/relocation.h>
#endif

#if EVEREST
#include <sys/EVEREST/evconfig.h>
#define	GETRTC	*((volatile long long*)EV_RTC)
#endif
#if defined(IP19) || defined(IP21) || defined(IP25)
#include <sys/EVEREST/dang.h>
#endif	/* defined (IP19) || defined (IP21) || defined(IP25) */

#include <ksys/rmap.h>		/* For SYSSGI= 1023, rmap_scantest	*/

#ifdef ULI
#include <ksys/uli.h>
#endif
#ifdef SN0
#include <sys/nodepda.h>

#define	GETRTC	GET_LOCAL_RTC

extern int nummodules;
extern int get_kmod_info(int , module_info_t *);
#endif
#ifdef SW_FAST_CACHE_SYNCH
#include <sys/sysmp.h>
#endif
/*
 * Definitions for NUMA  memory management
 */
#include <sys/pmo.h>
#include <sys/numa.h>

#if defined(IP30) && defined(DEBUG)
#include <sys/RACER/IP30.h>
#include <sys/RACER/racermp.h>
#endif
#if defined(CELL) && defined(CRED_DEBUG)
#include <ksys/cred.h>
#endif

time_t autopoweron_alarm;
int softpowerup_ok;		/* defaut to 0; set to 1 during boot on systems
				 * that support alarmclock powerup */

/* procscan handle to retrieve process group and session info */
struct scanhandle {
	pid_t	id;		/* group leader id or session id */
	pid_t	*useraddr;
	int	usermax;
	int	count;
};
/* procscan handle to retrieve proc info */

struct kiu_arg {
	uint64_t	kid;
	pid_t		pid;
	char		name[PSCOMSIZ];
};

extern int get_host_id(char *, int *, char *);

extern int getgroups(int, gid_t *, rval_t *);
extern int (*setgroupsp)(int, gid_t *);
extern uint _hpc3_write2;
extern int settimeofday(void *);

static int findash(proc_t *, void *, int);
static int findgrp(proc_t *, void *, int);
static int findses(proc_t *, void *, int);
static int kid_is_uthread(proc_t *, void *, int);

extern int elfmap(int, Elf32_Phdr *, int, rval_t *);
extern int elf64map(int, Elf64_Phdr *, int, rval_t *);
extern int set_nvram(char *, char *);
extern void sys_setled(int);

struct sigaltstacka;
extern int sigaltstack(struct sigaltstacka *);
struct sigstacka;
extern int sigstack(struct sigstacka *);

extern int setsid(void *, rval_t *);
extern int setpgid(pid_t, pid_t);
extern int netproc(void);
extern int xlv_tab_set(void *, void *, int);
extern int xlv_next_config_request(void *);
extern int xlv_attr_get_cursor(void *);
extern int xlv_attr_get(void *, void *);
extern int xlv_attr_set(void *, void *);
extern int open_by_handle (void *, size_t, int, rval_t *);
extern int readlink_by_handle (void *, size_t, void *, size_t, rval_t *);
extern int btool_size(void);
extern int btool_getinfo(void *);
extern void btool_reinit(void);
#ifdef _MEM_PARITY_WAR
extern int parity_gun(int, void *p, rval_t *);
#endif /* _MEM_PARITY_WAR */
#if (IP26 || IP28 || IP32) && DEBUG
extern int ecc_gun(int, void *p, rval_t *);
#endif /* IP26 || IP28 || IP32 */
/* KERNEL_ASYNCIO */

extern unsigned int update_ust(void);

#ifdef DATAPIPE
extern int fspe_bind(caddr_t);*/
#endif

#ifdef EVEREST
extern void get_pwr_fail(char *v);
extern void set_pwr_fail(uint p);
extern void get_fru_nvram(char* buffer);
extern void set_fru_nvram(char* buffer, int len);
extern int clr_fru_nvram(void);
extern int record_uptime;
#endif 

#if _MIPS_SIM == _ABI64
int rusage_to_irix5(void *, int , xlate_info_t *);
static int invent_to_irix5(void *, int , xlate_info_t *);
static int irix5_to_iospace(enum xlate_mode, void *, int, xlate_info_t *);
#endif
static void update_prda(void);

extern int get_mbufconst(void *, int, int *);
extern int get_pteconst(void *, int, int *);
extern int get_pageconst(void *, int, int *);
extern int get_paramconst(void *, int, int *);
extern int get_scachesetassoc(int *);
extern int get_scachesize(int *);

#if defined(DEBUG) || defined(INDUCE_IO_ERROR)
extern int xfs_errortag_add(int, int64_t);
extern int xfs_errortag_clear(int, int64_t);
extern void xfs_errortag_clearall(int64_t);
#endif

#if R10000
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#endif

extern int nfs_notify(char *addr, int addrlen, rval_t *rvp);
extern int cachefs_sys(int, caddr_t, rval_t *);
extern int kthread_syscall (sysarg_t, sysarg_t, rval_t *);
extern int lockd_sys(char *addr, int len, rval_t *rvp);

extern int ckpt_sys(sysarg_t, sysarg_t, sysarg_t, sysarg_t, sysarg_t, rval_t *);

#ifdef SN0
extern int pcitype_graph_match(int, int, int, int);
extern int part_operation(int, int, void *, void *, rval_t *);
extern int gather_craylink_routerstats;
extern void set_part_reboot_nvram(char *val) ;
#endif

#ifdef	DEBUG
extern	int	lpage_syscall(sysarg_t,sysarg_t,sysarg_t,rval_t *);
extern	int	map_lpage_syscall(size_t, caddr_t, size_t, rval_t *);
#endif

extern int	chproj(char *fname, enum symfollow, sysarg_t p, rval_t *rvp);
extern int	fchproj(int fd, sysarg_t p, rval_t *rvp);

/*
 * We need this allocated statically so we don't blow the kernel stack.
 * Note that the name is 128 characters. We don't have a kernel variable
 * with a name longer than this (nor should we). We allocate up to 4K
 * for NVRAM data since the FRU analysis will need at least this much.
 * These variables are used by the syssgi() GET/SET_NVRAM routines.
 */
static char nv_name[128];
static char nv_val[SGI_NVSTRSIZE];


int upanic_flag = 0;



/*
 * map_prda - used to fill in all kernel fields in the prda that
 * are maintained regardless of whether the prda is locked or not.
 * Thus this is called when a prda has been swapped back in or on first fault.
 * Many fields are only maintained if the prda is locked - we don't
 * have to muck with those.
 * Since the prda isn't locked when this is called, we must temporarily
 * map it in to a kernel address
 */
void
map_prda(pfd_t *pfd, caddr_t vaddr)
{
	uthread_t *ut = curuthread;
	caddr_t		tmp_va;

	ASSERT(ut);

	tmp_va = page_mapin(pfd, VM_VACOLOR, colorof(vaddr));
	((struct prda *)tmp_va)->t_sys.t_pid = current_pid();
	((struct prda *)tmp_va)->t_sys.t_rpid = (pid_t) ut->ut_id;
	((struct prda *)tmp_va)->t_sys.t_fpflags =
					ut->ut_pproxy->prxy_fp.pfp_fpflags;
	((struct prda *)tmp_va)->t_sys.t_prid = private.p_cputype_word;

	((struct prda *)tmp_va)->t_sys.t_cpu = cpuid();
	page_mapout(tmp_va);
}

/*
 * updates prda fields that kernel maintains. Only currently used for
 * fp fields for now
 * This is always called at top of syscall path so it it safe to
 * just use suword, etc.
 */
static void
update_prda(void)
{
	/*
	 * can't use ut->ut_prda pointer unless you have called lockprda().
	 * NOTE: ut->ut_pproxy->prxy_fp.pfp_fpflags is a byte, but
	 * t_fpflags is a word.
	 */
	suword(&((struct prda *)PRDAADDR)->t_sys.t_fpflags,
			curuthread->ut_pproxy->prxy_fp.pfp_fpflags);
}

#ifdef TW_DEBUG
mutex_t	timed_mutex;
sv_t	timed_sv;
#endif
#if defined (SN0)  && defined (FORCE_ERRORS)
extern int error_induce(sysarg_t *, rval_t *);
#endif

/* The only reason these helper functions exist is to keep syssgi() stack
 * consumption down.  The current compilers [7.2] do not allocate seperate
 * stack frames for nested blocks, so stack space for *all* the variables
 * defined in the various case blocks was being allocated on any call to 
 * syssgi().  This was causing k_stack overflows.
 */
static int syssgi_hostident(struct syssgia *uap);
static int syssgi_tune(struct syssgia *uap);
static int syssgi_nvram(struct syssgia *uap);
static int syssgi_rusage(struct syssgia *uap, int abi);
#ifdef IP32
static int syssgi_ip32(struct syssgia *uap);
#endif 
static int syssgi_dba_conf(struct syssgia *uap);
#ifdef R10000
static int syssgi_eventctr(struct syssgia *uap, rval_t *rvp);
#endif
static int syssgi_ash(struct syssgia *uap, rval_t *rvp);
static int syssgi_tstamp(struct syssgia *uap, rval_t *rvp);


int
syssgi(struct syssgia *uap, rval_t *rvp)
{
	int	error = 0;
	uthread_t *ut = curuthread;
	int	abi = get_current_abi();
	extern	int cumount(char *, int);

#ifdef DEBUG
	void mutex_test_init(void);
	void mutex_tester_init(void);
	int mutex_test(int distance);
#endif
#if defined(INDUCE_IO_ERROR)	
	extern int xlv_insert_error, xlv_error_freq,
	dksc_error_freq, dksc_insert_error;
	extern dev_t dksc_error_dev, xlv_error_dev2;
#endif	
	void sv_context_switch(int count);
	void mri_test_init(void);
	void mri_test_dolock(int, int);

	/* record command for sat */
	_SAT_SET_SUBSYSNUM(uap->cmd);

	switch (uap->cmd) {
#ifdef ULI
	case SGI_ULI:
		switch (uap->arg1) {
		case ULI_SLEEP:
			return uli_sleep(uap->arg2, uap->arg3);
		case ULI_WAKEUP:
			return uli_wakeup_lock(uap->arg2, uap->arg3);
		}
		break;
#endif

	case SGI_READB:
	case SGI_WRITEB:
		{
		extern int 	syssgi_rdwrb(int, int, char *,  usysarg_t, 
					     sysarg_t, sysarg_t, sysarg_t, 
					     rval_t *);
		off_t		blkno;
		usysarg_t 	nblks;
		int		mode;
		int		fdes;
		vfile_t		*fp;
		off_t		offset;
		sysarg_t 	off1, off2, off64;
		
		mode = uap->cmd == SGI_READB ? FREAD : FWRITE;
		fdes = uap->arg1;
		blkno = uap->arg3;
		nblks = uap->arg4;

		if (error = getf(fdes, &fp))
			return error;
		if ((fp->vf_flag & mode) == 0)
			return EBADF;

		/* These calls operate only on char special files. */
		if (!VF_IS_VNODE(fp) || VF_TO_VNODE(fp)->v_type != VCHR)
			return EINVAL;
		
		/*
		 * o32 and n32 kernels will put the off_t in
		 * off1 and off2 if the caller is n32 because
		 * both kernels have only 32 bit sysarg_t's.
		 */  
#if (_MIPS_SIM == _ABI64)
		if (ABI_HAS_64BIT_REGS(abi)) {
			off64 = BBTOOFF(blkno);
			off1 = off2 = 0;
		} else 
#endif
		{
			if (ABI_IS_IRIX5_N32(abi)) {
				blkno = ((off_t) uap->arg4 << 32) |
					(off_t) uap->arg5 & 0xffffffff;
				nblks = uap->arg6;
		        }
			offset = BBTOOFF(blkno);
			off1 = (offset >> 32) & 0xffffffff;
			off2 = offset & 0xffffffff;
			off64 = 0;
		}	

		if (blkno < 0 || nblks == 0)
			return EINVAL;
		
		/*
		 * Use the vncalls function _read/_write to do the dirty
		 * work for us. This is hacky, and we had to fake the
		 * args to look like pread/pwrite syscall, but hey, it works.
		 */
		error = syssgi_rdwrb(mode, fdes, (char *)uap->arg2,
				 BBTOB(nblks), off64, off1, off2, rvp); 
		_SAT_FD_RDWR(fdes, mode, error);

		break;
	    }

	case SGI_SYSID:
	case SGI_MODULE_INFO:
		return syssgi_hostident(uap);

	case SGI_NUM_MODULES:
#ifdef SN0
		if (nummodules > 0)
			rvp->r_val1 = nummodules;
		else
			return ENXIO;
#else
		rvp->r_val1 = 1;
#endif
		break;


	case SGI_RDNAME:
	{
		vproc_t	*vpr;
		pid_t	pid = uap->arg1;
		caddr_t	base = (caddr_t)uap->arg2;
		char	comm[PSCOMSIZ];

		/*
		 * Read user name for user.
		 * Args: pid, addr-of-name, size-of-name
		 */
		if (uap->arg3 < 0)
			return EINVAL;
		rvp->r_val1 = min(uap->arg3, PSCOMSIZ);

		if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return ESRCH;

		VPROC_GETCOMM(vpr, get_current_cred(), comm, rvp->r_val1,
				error);
		if (!error) {
			if (copyout(comm, base, rvp->r_val1) < 0)
				error = EFAULT;
		}
		
		VPROC_RELE(vpr);
		break;
	}

	case SGI_BUFINFO:
	{
		int which_buf = uap->arg1;
		int howmany_bufs = uap->arg2;
		caddr_t ubase = (caddr_t)uap->arg3;
		extern int getbuf_stats(int, int, caddr_t, __int64_t *);

		error = getbuf_stats(which_buf, howmany_bufs, ubase,
					&rvp->r_val1);
		break;
	}

	case SGI_TUNE_SET:
	case SGI_TUNE_GET:
		return syssgi_tune(uap);

	case SGI_SPIPE:
		CURVPROC_SET_PROXY(VSETP_SPIPE, uap->arg1, 0);
		break;
	
	case SGI_MINRSS:
#if NEVER
		error = syscall_minrss(uap->arg1, uap->arg2, uap->arg3,
				uap->arg4, uap->arg5);
#endif
		error = EINVAL;
		break;

#ifdef DEBUG
	/*
	 * Hooks used to test for Heap memory . 
	 */
	case 1002:
		if (uap->arg1 > 100000)
			return EINVAL;
		if (uap->arg2 & VM_NODESPECIFIC)
			rvp->r_val1 = (__psint_t)kmem_alloc_node(uap->arg1, uap->arg2, cnodeid());
		else 
			rvp->r_val1 = (__psint_t)kmem_alloc(uap->arg1, uap->arg2);
		break;

	case 1003:
		if (uap->arg1 > 0)
			return EINVAL;
		if (uap->arg2 > 100000)
			return EINVAL;
		if (uap->arg3 & VM_NODESPECIFIC)
			rvp->r_val1 = (__psint_t)kmem_realloc_node(
					(void *) uap->arg1, 
					uap->arg2, uap->arg3, cnodeid());
		else 
			rvp->r_val1 = (__psint_t)kmem_realloc(
						(void *) uap->arg1, 
						uap->arg2, uap->arg3);
		break;

	case 1004:
		if (uap->arg1 > 0)
			return EINVAL;
		kern_free((void *)uap->arg1);
		break;

	case 1007:
	{
		auto struct vfile *fp;
		/*
		 * set fd to arbitrary, possibly negative,
		 * offset (say, for /dev/kmem)
		 */
		if (uap->arg1 < 0)
			return EINVAL;
		if (error = getf(uap->arg1, &fp))
			return error;

		VFILE_SETOFFSET(fp, uap->arg2);
		break;
	}

	case 1009:
	{
		/* arg1 is the kernel buffer, and arg2 is the user buffer,
		 * and ar3 is the size of user buffer. 
		 * Do a copy in from user buffer to kernel buffer. 
		 */
		if (copyin((void *)uap->arg2, (void *)uap->arg1, uap->arg3))
			error = EFAULT;
		else
			rvp->r_val1 = uap->arg3;
		break;
	}

	case 1010:
	{
		/* arg1 is the kernel buffer, and arg2 is the user buffer,
		 * and ar3 is the size of user buffer. 
		 * Do a copy in from user buffer to kernel buffer. 
		 */
		if (copyout((void *)uap->arg1, (void *)uap->arg2, uap->arg3))
			error = EFAULT;
		else 
			rvp->r_val1 = uap->arg3;
		break;

	}

	case 1005:
			panic("syssgi test panic");
			/* For those brave hearts forcing a return from panic */
			break;
	case 1006:
			ASSERT(0);
			break;	/* can continue out of _assfail() */
#ifdef R10000		/* recoverable ECC insertion for debuging ECC hndlr */
	case 1008:
	{
			void try_icache_err(int);
			try_icache_err(uap->arg1);
			break;
	}
#endif
#endif /* DEBUG */

#ifdef KV_DEBUG
	case 1021:			/* sptalloc(size, flags, color) */
		{
		__psunsigned_t addr;
		if (uap->arg1 > v.v_maxdmasz)
			return ENOMEM;

		/*
		 * Value returned is page offset into first kernel
		 * virtual address, so caller doesn't have to be
		 * compiled 64-bit to work on 64-bit kernel.
		 */
		while ((addr =
			sptalloc(uap->arg1, uap->arg2, uap->arg3)) == NULL) {
			ASSERT(uap->arg2 & VM_NOSLEEP);
#ifdef VM_BREAKABLE_maybe
			if (uap->arg2 & VM_BREAKABLE)
				break;
#endif
			sptwait(PWAIT);
		}
		addr -= sptbitmap.m_unit0;
		rvp->r_val1 = addr;
		break;
		}

	case 1022:			/* sptfree(size, vaddr) */
		{
		__psunsigned_t addr = uap->arg1;
		addr += sptbitmap.m_unit0;

		if (uap->arg2 > v.v_maxdmasz)
			return EINVAL;
		sptfree(uap->arg2, addr);
		break;
		}


	case 1023:			/* kvpalloc(size, flags, color) */
		{
		__psunsigned_t addr;
		if (uap->arg1 > v.v_maxdmasz)
			return ENOMEM;

		/*
		 * Value returned is page offset into first kernel
		 * virtual address, so caller doesn't have to be
		 * compiled 64-bit to work on 64-bit kernel.
		 */
		while ((addr = (__psunsigned_t)
			kvpalloc(uap->arg1, uap->arg2, uap->arg3)) == NULL) {
			ASSERT(uap->arg2 & VM_NOSLEEP);
		}
		addr = btoc(addr);
		addr -= sptbitmap.m_unit0;
		rvp->r_val1 = addr;
		break;
		}

	case 1024:			/* kvpfree(vaddr, size) */
		{
		__psunsigned_t addr = uap->arg1;
		addr += sptbitmap.m_unit0;
		addr = ctob(addr);

		if (uap->arg2 > v.v_maxdmasz)
			return EINVAL;
		kvpffree((void *)addr, uap->arg2, 0);
		break;
		}

	case 1025:			/* kvswappable(vaddr, size) */
		{
		__psunsigned_t addr = uap->arg1;
		addr += sptbitmap.m_unit0;
		addr = ctob(addr);

		if (uap->arg2 > v.v_maxdmasz)
			return EINVAL;
		kvswappable((void *)addr, uap->arg2);
		break;
		}

	case 1026:			/* kvpswapin(vaddr, size, flags) */
		{
		__psunsigned_t addr = uap->arg1;
		addr += sptbitmap.m_unit0;
		addr = ctob(addr);

		if (uap->arg2 > v.v_maxdmasz)
			return EINVAL;
		kvswapin((void *)addr, uap->arg2, 0);
		break;
		}

	case 1027:			/* store(vaddr, size, bits) */
		{
		__psunsigned_t addr = uap->arg1;
		register char *sp, c;
		register int size;

		addr += sptbitmap.m_unit0;
		addr = ctob(addr);

		size = uap->arg2;
		if (size > v.v_maxdmasz)
			return EINVAL;

		sp = (char *)addr;
		c = uap->arg2;
		while (--size >= 0)
			*sp++ = c;
		break;
		}
#endif /* KV_DEBUG */
#ifdef TW_DEBUG
	case 1028:		/* sv_timedwait testing */
		{
		timespec_t ts, rts;
		time_t ticks = uap->arg1;

		tick_to_timespec(uap->arg1, &ts, NSEC_PER_TICK);
		mutex_lock(&timed_mutex, PZERO);
		sv_timedwait(&timed_sv, 0, &timed_mutex, 0, 1, &ts, &rts);
		break;
		}
#endif /* TW_DEBUG */
#ifdef DEBUG_INTR_TSTAMP
#if defined(EVEREST) || defined(SN0)
	case 1029:
		{
		extern long long dintr_tstamp( int, int, int *, int *);
		extern int dintr_tstamp_noithread( void );
		extern int dintr_tstamp_noistack( void );
		int stat=0;

		struct {
			int opcode;
			int destcpu;
			int intrcnt;
			int maxtime;
			int mintime;
			long long totaltime;
		} argblk;

		if (copyin((caddr_t)uap->arg1, &argblk, sizeof(argblk)))
			return EFAULT;
		
		if (argblk.opcode == 0) {
			argblk.totaltime =
				dintr_tstamp( argblk.destcpu, argblk.intrcnt,
					&argblk.maxtime, &argblk.mintime);
		
			if (copyout(&argblk, (caddr_t) uap->arg1, sizeof(argblk)))
				return EFAULT;
			break;
		} else if (argblk.opcode == 1)
			stat = dintr_tstamp_noithread();
		else if (argblk.opcode == 2)
			stat = dintr_tstamp_noistack();
		else if (argblk.opcode == 3) {
			long long ts1, ts2;
			vasid_t vasid;
			pas_t *pas;

			as_lookup_current(&vasid);
			pas = VASID_TO_PAS(vasid);

			ts1 = GETRTC;
			tlbsync(BHV_TO_VAS(&pas->pas_bhv)->vas_gen,
				pas->pas_isomask,
				FLUSH_MAPPINGS|FLUSH_WIRED);
			ts2 = GETRTC;

			argblk.totaltime = ts2 - ts1;
			if (copyout(&argblk, (caddr_t) uap->arg1, sizeof(argblk)))
				return EFAULT;
			break;
		} else if (argblk.opcode == 4) {
			long long ts1, ts2;

			ts1 = GETRTC;
			tlbsync(0LL, allclr_cpumask, FLUSH_MAPPINGS);
			ts2 = GETRTC;

			argblk.totaltime = ts2 - ts1;
			if (copyout(&argblk, (caddr_t) uap->arg1, sizeof(argblk)))
				return EFAULT;
			break;
		} else if (argblk.opcode == 5) {
			long long ts1, ts2;
			vasid_t vasid;
			pas_t *pas;

			as_lookup_current(&vasid);
			pas = VASID_TO_PAS(vasid);

			ts1 = GETRTC;
			tlbclean( pas, /* vpn */ 0, allclr_cpumask);
			ts2 = GETRTC;

			argblk.totaltime = ts2 - ts1;
			if (copyout(&argblk, (caddr_t) uap->arg1, sizeof(argblk)))
				return EFAULT;
			break;
		}
		if (stat)
			error = EINVAL;
		break;
		}	/* case 1029 */
#endif /* EVEREST || SN0 */
#endif /* DEBUG_INTR_TSTAMP */
#if defined(CELL) && defined(CRED_DEBUG)
	case 500:
		{
		credid_t credid;

		credid = cred_getid(get_current_cred());
		rvp->r_val1 = (__int64_t)credid;
		break;
		}
	case 501:
		{
		cred_t *credp;

		credp = credid_getcred((credid_t)uap->arg1);
		if (credp) {
			rvp->r_val1 = credp->cr_uid;
			printf("cred ref cnt %d\n", credp->cr_ref);
			crfree(credp);
		} else
			rvp->r_val1 = -1;
		break;
		}
	case 502:
                VHOST_CREDFLUSH(cellid());
		break;
#endif
	/*
	 * The following gives a physical page number for a specified
	 * user address.  Arg1 is the user address, arg2 is a pointer
	 * to an int where the page number is put.
	 */
	case SGI_PHYSP:
	{
		pfn_t pfn;
		if (!vtop((caddr_t) uap->arg1, 4, &pfn, 1) )
			error = EINVAL;
		else if (copyout( &pfn, (caddr_t) uap->arg2, sizeof(pfn)))
			error = EFAULT;
		break;
	}

	case SGI_IDBG:		/* internal debugger support */
	{
		if (!cap_able(CAP_SYSINFO_MGT))
			return EPERM;
#if _MIPS_SIM == _ABI64
		if (!ABI_IS_IRIX5_64(abi))
			return EINVAL;
#endif
		switch (uap->arg1) {
		case 0:
			error = idbg_copytab((char *)uap->arg2, uap->arg3, rvp);
			break;
		case 1:
			error = idbg_dofunc((struct idbgcomm *)uap->arg2, rvp);
			break;
		case 2:
			idbg_switch(uap->arg2, uap->arg3);
			break;
		case 3:
			idbg_tablesize(rvp);
			break;
		default:
			return EINVAL;
		}
		break;
	}

	/*
	 * Hardware Inventory.
	 *
	 * Usage:
	 *	rc = syssgi(SGI_INVENT, SGI_INV_SIZEOF)
	 * or
	 *	syssgi(SGI_INVENT, SGI_INV_READ, buffer, length)
	 *
	 */
	case SGI_INVENT:

		switch (uap->arg1) {
		case SGI_INV_SIZEOF:
			rvp->r_val1 = get_sizeof_inventory(abi);
			break;
		case SGI_INV_READ:
		{
			caddr_t buffer = (caddr_t) uap->arg2;
			inventory_t *curr = 0;
			invplace_t iplace = INVPLACE_NONE;
			int count  = uap->arg3;
			int isize = get_sizeof_inventory(abi);

			while ((curr = get_next_inventory(&iplace)) != 0
				&& count >= isize) {
				if (XLATE_COPYOUT(curr, buffer, isize,
				    invent_to_irix5, abi, 1))
					return EFAULT;
				count  -= isize;
				buffer += isize;
			}

			rvp->r_val1 = uap->arg3 - count;
			break;
		}
		default:
			return EINVAL;
		}
		break;

	case SGI_SETNVRAM:
	case SGI_SETKOPT:
	case SGI_GETNVRAM:
		return syssgi_nvram(uap);

	/*
	 * Set CPU Board Leds
	 */
	case SGI_SETLED:
#if defined (SN0)
		if (!cap_able(CAP_DEVICE_MGT))
			return EPERM;
#endif
		sys_setled(uap->arg1);
		break;

	case SGI_QUERY_CYCLECNTR:
	{
		__psint_t addr;
		uint cycle;
		int poffmask = NBPC - 1;
		if (copyin((caddr_t)uap->arg1, &cycle, sizeof(uint)))
			return EFAULT;
		if ((addr = query_cyclecntr(&cycle)) == 0)
			return ENODEV;
		if (copyout(&cycle, (caddr_t)uap->arg1, sizeof(uint)))
			return EFAULT;
		rvp->r_val1 = RTC_MMAP_ADDR | (addr & poffmask);
		break;
	}
	case SGI_CYCLECNTR_SIZE:
	        rvp->r_val1 = query_cyclecntr_size();
		break;

        case SGI_QUERY_FASTTIMER:
	        rvp->r_val1 = query_fasttimer();
		break;

	case SGI_QUERY_FTIMER:
	{
#if !defined(CLOCK_CTIME_IS_ABSOLUTE)
		extern callout_info_t *fastcatodo;

		if (CI_TODO_NEXT(fastcatodo))
                	rvp->r_val1 = 1;
		else
#endif /* !defined(CLOCK_CTIME_IS_ABSOLUTE) */
                	rvp->r_val1 = 0;
                break;
	}

	/*
	 * tstamp event interface.
	 */
	case SGI_RT_TSTAMP_CREATE:
        case SGI_RT_TSTAMP_DELETE:
        case SGI_RT_TSTAMP_START:		/* deprecated */
        case SGI_RT_TSTAMP_STOP:		/* deprecated */
        case SGI_RT_TSTAMP_ADDR:
        case SGI_RT_TSTAMP_MASK:
        case SGI_RT_TSTAMP_EOB_MODE:
        case SGI_RT_TSTAMP_WAIT:
	case SGI_RT_TSTAMP_UPDATE:
		error = syssgi_tstamp(uap, rvp);
		break;
	
	case SGI_SSYNC:
		VHOST_SYNC(SYNC_ATTR|SYNC_FSDATA|SYNC_DELWRI|SYNC_WAIT);
		break;

	case SGI_BDFLUSHCNT:
	{
		static int	last_holdoff = 0;
		extern timespec_t bdflush_holdoff;
		extern lock_t	bdflush_holdoff_lock;
		extern int	vfssynccnt;
		extern int 	bdflush_interval;
		int 		bdflush_delay = uap->arg1;
		timespec_t	now, last_scheduled_run;
		int		s;
		cpu_cookie_t	cpuid;

		if (!cap_able(CAP_SCHED_MGT))
			return EPERM;

		nanotime(&now);

		s = mutex_spinlock(&bdflush_holdoff_lock);
		last_scheduled_run = bdflush_holdoff;
		bdflush_holdoff.tv_sec = now.tv_sec + bdflush_delay;
		mutex_spinunlock(&bdflush_holdoff_lock, s);

		/*
		 * We need to return the old flush delay.  If this is
		 * the first SGI_BDFLUSHCNT call, or if the interval
		 * specified by the last call has expired, return
		 * the default value.
		 */ 
		if ((last_holdoff == 0) || 
		    (now.tv_sec > last_scheduled_run.tv_sec) ||
		    ((now.tv_sec == last_scheduled_run.tv_sec) &&
		     (now.tv_nsec >= last_scheduled_run.tv_nsec))) {
			/*
			 * Return value is expected in seconds, so round
			 * up to nearest second.
			 */
			last_holdoff = ((bdflush_interval - 1) / HZ) + 1;
		}

		rvp->r_val1 = last_holdoff;
		cpuid = setmustrun(clock_processor);
		s = splhi();
		last_holdoff = vfssynccnt = bdflush_delay;
		splx(s);
		restoremustrun(cpuid);
#if defined(DEBUG)
		cmn_err(CE_WARN, "Rescheduled bdflush for %ds from now\n",
			bdflush_delay);
#endif /* DEBUG */
		break;
	}

	case SGI_SIGALTSTACK:
		/*
		 * sigaltstack looks at the 1st two args
		 * massage the syssgi args to look that way
		 */
		uap->cmd = uap->arg1;
		uap->arg1 = uap->arg2;

		return sigaltstack((struct sigaltstacka *)uap);

	case SGI_SIGSTACK:
		/*
		 * sigstack looks at the 1st two args
		 * massage the syssgi args to look that way
		 */
		uap->cmd = uap->arg1;
		uap->arg1 = uap->arg2;

		return sigstack((struct sigstacka *)uap);

	case SGI_SETGROUPS:
		/* ptr-to-function permits AFS to replace setgroups */
		return (*setgroupsp)((int)uap->arg1, (gid_t *)uap->arg2);

	case SGI_GETGROUPS:
		return getgroups((int)uap->arg1, (gid_t *)uap->arg2, rvp);

	case SGI_TITIMER:
		return setitimer_value((int)uap->arg1,
				   (struct timeval *)uap->arg2,
				   (struct timeval *)uap->arg3);

	case SGI_SETSID:
		return setsid((void *)uap, rvp);

	case SGI_SETPGID:
		error = setpgid(uap->arg1, uap->arg2);

		_SAT_PROC_ACCESS(SAT_PROC_ATTR_READ, uap->arg1,
				get_current_cred(), error, -1);
		return error;

	case SGI_GETPGID:
	case SGI_GETSID:
	{
		vproc_t 	*vpr;
		pid_t		pid;
		vp_get_attr_t	attr;

		pid = uap->arg1;
		if (pid < 0 || pid >= MAXPID)
			return EINVAL;

		if (pid == 0)
			pid = current_pid();

		if (pid == current_pid())
			vpr = curvprocp;
		else if ((vpr = VPROC_LOOKUP(pid)) == NULL) {
			_SAT_PROC_ACCESS(SAT_PROC_ATTR_READ, pid, NULL,
					 ESRCH, -1);
			return ESRCH;
		}

		VPROC_GET_ATTR(vpr, VGATTR_PGID | VGATTR_SID, &attr);

		if (pid != current_pid())
			VPROC_RELE(vpr);

		if (uap->cmd == SGI_GETSID)
			rvp->r_val1 = attr.va_sid;
		else
			rvp->r_val1 = attr.va_pgid;

		_SAT_PROC_ACCESS(SAT_PROC_ATTR_READ, pid, get_current_cred(),
				 0, -1);
		break;
	}

        case SGI_SETASH:
	case SGI_GETASH:
	case SGI_SETPRID:
	case SGI_GETPRID:
	case SGI_SETSPINFO:
	case SGI_GETSPINFO:
	case SGI_NEWARRAYSESS:
	case SGI_GETDFLTPRID:
	case SGI_PIDSINASH:
	case SGI_GETGRPPID:
	case SGI_GETSESPID:
		return syssgi_ash(uap,rvp);

        case SGI_CHPROJ:
		return chproj((char *)uap->arg1, FOLLOW, uap->arg2, rvp);

        case SGI_LCHPROJ:
		return chproj((char *)uap->arg1, NO_FOLLOW, uap->arg2, rvp);

	case SGI_FCHPROJ:
		return fchproj((int)uap->arg1, uap->arg2, rvp);

	case SGI_SYSCONF:
		return sysconf((int)uap->arg1, rvp);

	case SGI_PATHCONF:
		switch (uap->arg3) {
		case PATHCONF: {
			struct pathconfa pca;

			pca.fname = (char *) uap->arg1;
			pca.name = uap->arg2;
			return pathconf(&pca, rvp);
		}
		case FPATHCONF: {
			struct fpathconfa fpca;

			fpca.fdes = uap->arg1;
			fpca.name = uap->arg2;
			return fpathconf(&fpca, rvp);
		}
		}
		break;

	case SGI_SETTIMEOFDAY:
		return settimeofday((void *)uap->arg1);

	case SGI_SETTIMETRIM: {
		/*REFERENCED*/
		cpu_cookie_t ocpuid;
		int rval;

		ocpuid = setmustrun(clock_processor);
		rval =  settimetrim(uap->arg1);
		restoremustrun(ocpuid);
		return(rval);
		}

	case SGI_GETTIMETRIM:
	    {
		int t = gettimetrim();
		if (copyout(&t, (void *)uap->arg1, sizeof t))
			return EFAULT;
		return 0;
	    }

	case SGI_SPROFIL:
		return sprofil((void *)uap->arg1, uap->arg2,
			       (void *)uap->arg3, uap->arg4, 1);

	case SGI_RUSAGE:
		return syssgi_rusage(uap, abi);

	case SGI_NETPROC:
#ifdef _MP_NETLOCKS
		return(EINVAL);
#else
		return netproc();
#endif

	case SGI_NFSCNVT: {
		auto int fd;

		/*
		 * The deprecated format was:
		 *	       cmd	    arg1   arg2
		 *	syssgi(SGI_NFSCNVT, &fh,   mode)
		 * which we will still support (just in case).
		 *
		 * The supported (new) format is:
		 *	       cmd	    arg1    arg2   arg3
		 *	syssgi(SGI_NFSCNVT, fhvers, &fh,   mode)
		 * which we recognize by noting that arg1 is either
		 * 2 or 3 or it must be the old format.
		 */
		if (uap->arg1 == 2) {		/* new fmt, V2 fhandle */
			error = nfs_cnvt((void *)uap->arg2, uap->arg3, &fd);
		} else if (uap->arg1 == 3) {	/* new fmt, V3 fhandle */
			error = nfs_cnvt3((void *)uap->arg2, uap->arg3, &fd);
		} else {			/* assume old fmt, V2 */
			error = nfs_cnvt((void *)uap->arg1, uap->arg2, &fd);
		}
		if (!error) {
			rvp->r_val1 = fd;
		}
		break;
	}

        case SGI_IOPROBE:
		/* XXX The detailed impl should be moved to io subsystem */
		/* XXX Old IO infrastructures should be replaced later */
		{
		iospace_t iospace;
		int 	  rvalue;

#if SN0
		char provider_name[30];
#else
		piomap_t *piomap;
		int	  err;
#endif

		/* Get the content of the IO space */
		if (COPYIN_XLATE((void*)uap->arg3, &iospace, sizeof(iospace),
				 irix5_to_iospace, abi, 1)) {
			error = EFAULT;
			break;
		}

		/*
		 * Make a check that [w]badaddr_val will not panic
		 * down the road.
		 */
		if ((iospace.ios_size != 1) && (iospace.ios_size != 2) &&
			(iospace.ios_size != 4) && (iospace.ios_size != 8)) {
			error = EINVAL;
			break;
		}

#if SN0
		if (uap->arg1 == ADAP_VME) {
			if (copyin((char *)uap->arg2, provider_name, 30)) {
				return(EFAULT);
			}
			error = vmeio_probe(provider_name, &iospace, &rvalue);
			if (error) {
				break;
			}
		}
		else {
			/*
			 * Probing of non-VME devices on SN0 is not
			 * supported now.
			 */
			error = ENODEV;
			break;
		}
#else /* !SN0 */

		piomap = pio_mapalloc(uap->arg1, uap->arg2, &iospace, 0,
			"IOPROBE");

		if (piomap == 0) {
			error = ENODEV;
			break;
		}

		if( uap->arg4 == IOPROBE_WRITE ) {
			err = pio_wbadaddr_val(piomap, iospace.ios_iopaddr, 
					       iospace.ios_size, uap->arg5);
		}
		else {
			err = pio_badaddr_val(piomap, iospace.ios_iopaddr,
					      iospace.ios_size, &rvalue); 
		}

		if (err) {
			pio_mapfree(piomap);
			error = ENODEV;
			break;
		}

		pio_mapfree(piomap);

#endif	/* SN0 */

		if ((uap->arg4 == IOPROBE_READ) && (uap->arg5 != 0) &&
		    (copyout((void *)&rvalue, (void *)uap->arg5, 
			     iospace.ios_size) < 0) ) {
			error = EFAULT;
		}

		break;
		}

	case SGI_NFSNOTIFY:
		error = nfs_notify((caddr_t)uap->arg1, (int)uap->arg2, rvp);
		break;

	case SGI_LOCKDSYS:
		error = lockd_sys((caddr_t)uap->arg1, (int)uap->arg2, rvp);
		break;

#if defined(IP19) || defined(IP21) || defined(IP25)
	case SGI_READ_DANGID:
		if (copyout(dang_id, (caddr_t)uap->arg1, uap->arg2))
			error = EFAULT;
		break;

#endif	/* defined (IP19) || defined (IP21) || defined (IP25) */

        case SGI_CONFIG:
		switch (uap->arg1) {
		case ADAP_READ:
			rvp->r_val1 = readadapters(uap->arg2);
			break;
		}
		break;

	case SGI_MCONFIG:
		
		return sgi_mconfig(uap->arg1, (void *)uap->arg2, uap->arg3,rvp);

	case SGI_SYMTAB:
		return sgi_symtab(uap->arg1, 
			(void *)uap->arg2, (void *)uap->arg3);

	case SGI_ELFMAP:
	{
		int retry;

		do {
			retry = 0;
#if _MIPS_SIM == _ABI64
			if (ABI_IS_64BIT(abi))
				error = elf64map(uap->arg1, 
						(Elf64_Phdr *)uap->arg2,
						uap->arg3, rvp);
			else
#endif
			error = elfmap((int)uap->arg1, (Elf32_Phdr *)uap->arg2,
				      (int)uap->arg3, rvp);
			if (error == EMEMRETRY) {
				if (vm_pool_wait(GLOBAL_POOL))
					retry = 1;
				else
					error = EAGAIN;
			}
		} while (retry);

		break;
	}
	case SGI_TOSSTSAVE:
		removetsave(0);
		break;

	case SGI_FDHI:
		rvp->r_val1 = fdt_gethi();
		break;

	case SGI_XLV_SET_TAB:
		return xlv_tab_set((void *)uap->arg1, (void *)uap->arg2, 1);

	case SGI_XLV_NEXT_RQST:
		return xlv_next_config_request((void *)uap->arg1);

	case SGI_XLV_ATTR_CURSOR:
		 return xlv_attr_get_cursor((void *)uap->arg1);

	case SGI_XLV_ATTR_GET:
		return xlv_attr_get((void *)uap->arg1, (void *)uap->arg2);

	case SGI_XLV_ATTR_SET:
		return xlv_attr_set((void *)uap->arg1, (void *)uap->arg2);

	case SGI_GRIO:
		return grio_config(uap->arg1, uap->arg2, uap->arg3, uap->arg4,
				   uap->arg5);

#if EVEREST
	case SGI_GET_EVCONF:
		if (copyout(EVCFGINFO, (void *)uap->arg1,
			    MIN(sizeof(evcfginfo_t), uap->arg2)))
			return EFAULT;
		break;

	/* usage syssgi(SGI_SBE_GET_INFO, slot, be[], ec[]); */
	case SGI_SBE_GET_INFO:
	{
		int be[MC3_NUM_LEAVES * MC3_BANKS_PER_LEAF];
		int ec[MC3_NUM_LEAVES * MC3_BANKS_PER_LEAF];

		mc3_get_sbe_count((uint)uap->arg1, be, ec);
		copyout(be, (uint *)uap->arg2, sizeof(be));
		copyout(ec, (uint *)uap->arg3, sizeof(ec));
		break;
	}

	/* usage syssgi(SGI_SBE_CLR_INFO, slot); */
	case SGI_SBE_CLR_INFO:
		mc3_clr_sbe_count((uint)uap->arg1);
		break;
#endif /* EVEREST */

	case SGI_SET_FP_PRECISE:
#if TFP
		/* Only TFP allows non-default settings of FP_PRECISE.
		 * All other platforms only have one non-selectable value.
		 */
		if (uap->arg1) {
			/* debug mode on */
			CURVPROC_SET_FPFLAGS(P_FP_IMPRECISE_EXCP,
				VFP_SINGLETHREADED | VFP_FLAG_OFF,
				error);
			if (!error)
				curexceptionp->u_eframe.ef_sr |= SR_DM;
		} else {
			/* debug mode off */
			CURVPROC_SET_FPFLAGS(P_FP_IMPRECISE_EXCP,
				VFP_SINGLETHREADED | VFP_FLAG_ON,
				error);
			if (!error)
				curexceptionp->u_eframe.ef_sr &= ~SR_DM;
		}
		update_prda();
#else
		error = EINVAL;
#endif /* TFP */
		break;

	case SGI_SET_FP_PRESERVE:
		if (uap->arg1) {
			CURVPROC_SET_FPFLAGS(P_FP_PRESERVE, VFP_FLAG_ON, error);
		} else {
			CURVPROC_SET_FPFLAGS(P_FP_PRESERVE, VFP_FLAG_OFF,error);
		}
		update_prda();
		break;

	case SGI_FETCHOP_SETUP:
#ifdef SN0
	{
		int err_val=0;
		vasid_t vasid;
		pas_t *pas;
		ppas_t *ppas;
		attr_t *attr;
		pde_t attrpde;
		register preg_t	*prp;
		register reg_t	*rp;
		register pde_t	*pt;
		uvaddr_t	end;
		auto pgno_t	sz;
		caddr_t vaddr = (caddr_t)uap->arg1;
		size_t len = uap->arg2;
		pgcnt_t bpcnt;


		as_lookup_current(&vasid);
		pas = VASID_TO_PAS(vasid);
		ppas = (ppas_t *)vasid.vas_pasid;
		mraccess(&pas->pas_aspacelock);
				
		pg_setfetchop(&attrpde);

		bpcnt = btoc(len);

		do {
			prp = findpreg(pas, ppas, vaddr);
			if (prp == NULL) {
				/* address not in process space */
				err_val = ENXIO;
				break;
			}
			rp = prp->p_reg;
			reglock(rp);
			
			if (!(rp->r_flags & RG_ANON)) {
				err_val = EFAULT;
				regrele(rp);
				goto end2;
			}

			sz = MIN(bpcnt, pregpgs(prp, vaddr));

			/*
			 * SPT
			 * Privatize shared PT's if necessary 
			 * (see comment in pmap.c)
			 */
			prp->p_nvalid += pmap_modify(pas, ppas, 
							prp, vaddr, sz);
		
			pt = pmap_ptes(pas, prp->p_pmap, vaddr, &sz, 0);

			if (pt == NULL) {
				err_val = ENOMEM;
				regrele(rp);
				goto end2;
			}
			ASSERT(sz >= 0 && sz <= bpcnt);
			bpcnt -= sz;
			end = vaddr + ctob(sz);

			attr = findattr_range(prp, vaddr, end);

			pmap_downgrade_addr_boundary(pas, prp, vaddr,
							sz, PMAP_TLB_FLUSH);
			for ( ; ; ) {
				ASSERT(attr);
				attr->attr_cc = attrpde.pte.pg_cc;
				attr->attr_uc = attrpde.pte.pg_uc;

				if (attr->attr_end == end)
					break;

				attr = attr->attr_next;
			}

			for ( ; --sz >= 0 ; pt++, vaddr += NBPC) {
				pg_setfetchop(pt);
			}
			regrele(rp);

		} while (bpcnt);

	      end2:
		mrunlock(&pas->pas_aspacelock);
		return err_val;

	}
#else
		return EPERM;
#endif

#if IP32
	case SGI_WRITE_IP32_FLASH:
		return syssgi_ip32(uap);
#endif

#ifdef SN0
	case SGI_ROUTERSTATS_ENABLED:
	        if(copyout(&gather_craylink_routerstats, 
			   (caddr_t) uap->arg1, sizeof(int)) < 0) 
		    error = EFAULT;
                break;
#endif

	case SGI_SET_CONFIG_SMM:
#if TFP
		if (uap->arg1) {
			/* SMM on */
			CURVPROC_SET_FPFLAGS(P_FP_SMM,
				VFP_FLAG_ON | VFP_SINGLETHREADED, error);
			if (!error)
				curexceptionp->u_eframe.ef_config |= CONFIG_SMM;
		} else {
			/* SMM off */
			CURVPROC_SET_FPFLAGS(P_FP_SMM,
				VFP_FLAG_OFF | VFP_SINGLETHREADED, error);
			curexceptionp->u_eframe.ef_config &= ~CONFIG_SMM;
		}
		update_prda();
#else
		error = EINVAL;
#endif /* TFP */
		break;

	case SGI_GET_FP_PRECISE:
#if TFP
		rvp->r_val1 = !(ut->ut_pproxy->prxy_fp.pfp_fpflags &
							P_FP_IMPRECISE_EXCP);
#else
		rvp->r_val1 = 0;
#endif
		break;

	case SGI_GET_CONFIG_SMM:
#if TFP
		rvp->r_val1 = (ut->ut_pproxy->prxy_fp.pfp_fpflags & P_FP_SMM);
#else
		rvp->r_val1 = 0;
#endif
		break;

	case SGI_FP_IMPRECISE_SUPP:
	case SGI_CONFIG_NSMM_SUPP:
#if TFP
		rvp->r_val1 = 1;
#else
		rvp->r_val1 = 0;
#endif
		break;

	case SGI_USE_FP_BCOPY:
		/*
		 * bcopy/bzero make this call (once per exec) to see if they
		 * should use the fpu for doing their job. For now, just check
		 * to see if we're on a tfp processor (it turns out that fp
		 * copy is just as fast whether we're in performance or precise
		 * mode). We might want to add other checks later.
		 */
#if TFP
		rvp->r_val1 = 1;
#else
		rvp->r_val1 = 0;
#endif
		
		break;

	case SGI_SPECULATIVE_EXEC:
		if (uap->arg1) {
			CURVPROC_SET_FPFLAGS(P_FP_SPECULATIVE,
					VFP_FLAG_ON, error);
		} else {
			CURVPROC_SET_FPFLAGS(P_FP_SPECULATIVE,
					VFP_FLAG_OFF, error);
		}
		break;

	case SGI_SET_DISMISSED_EXC_CNT:
		CURVPROC_SET_PROXY(VSETP_EXCCNT, uap->arg1, 0);
		break;

	case SGI_GET_DISMISSED_EXC_CNT:
		rvp->r_val1 = ut->ut_pproxy->prxy_fp.pfp_dismissed_exc_cnt;
		break;

	case SGI_GETLABEL:			/* MAC */
		return _MAC_GETLABEL((char *)uap->arg1, (mac_label *)uap->arg2);

	case SGI_SETLABEL:			/* MAC */
		return _MAC_SETLABEL((char *)uap->arg1, (mac_label *)uap->arg2);

	case SGI_GETPLABEL:			/* MAC */
		return _MAC_GETPLABEL((mac_label *)uap->arg1);

	case SGI_SETPLABEL:			/* MAC */
		return _MAC_SETPLABEL((mac_label *)uap->arg1, 1);

	case SGI_PLANGMOUNT:			/* EAG */
		return eag_mount((struct eag_mount_s *)uap->arg1, rvp,
		    (char *)uap->arg2);

	case SGI_PROC_ATTR_SET:
		return proc_attr_set((char *)uap->arg1, (char *)uap->arg2,
		    uap->arg3);

	case SGI_PROC_ATTR_GET:
		return proc_attr_get((char *)uap->arg1, (char *)uap->arg2);

	case SGI_REVOKE:			/* MAC */
		return _MAC_REVOKE((char *)uap->arg1);

	case SGI_ACL_GET:			/* ACL */
		return _ACL_GET((char *)uap->arg1, uap->arg2,
		    (struct acl *)uap->arg3, (struct acl *)uap->arg4);

	case SGI_ACL_SET:			/* ACL */
		return _ACL_SET((char *)uap->arg1, uap->arg2,
		    (struct acl *)uap->arg3, (struct acl *)uap->arg4);

	case SGI_MAC_GET:			/* MAC */
		return _MAC_GET((char *)uap->arg1, uap->arg2,
		    (mac_label *)uap->arg3);

	case SGI_MAC_SET:			/* MAC */
		return _MAC_SET((char *)uap->arg1, uap->arg2,
		    (mac_label *)uap->arg3);

	case SGI_CAP_GET:			/* CAP */
		return cap_get((char *)uap->arg1, uap->arg2,
		    (struct cap_set *)uap->arg3);

	case SGI_CAP_SET:			/* CAP */
		return cap_set((char *)uap->arg1, uap->arg2,
		    (struct cap_set *)uap->arg3);

	case SGI_SATREAD:			/* SAT */
		return _SAT_READ((char *)uap->arg1, (unsigned)uap->arg2, rvp);

	case SGI_SATWRITE:			/* SAT */
		return _SAT_WRITE(uap->arg1, uap->arg2, (char *)uap->arg3,
			(unsigned)uap->arg4);

	case SGI_SATCTL:			/* SAT */
		return _SAT_CTL(uap->arg1, uap->arg2, uap->arg3, rvp);

	case SGI_RXEV_GET: {
		/*
		 * Return 1 if it's unsafe for libc/rld to allow usage of
		 * _RLD_ROOT, _RLD_PATH, LD_LIBRARY_PATH, etc. environment
		 * variables; otherwise return 0.  It is unsafe to use these
		 * variables if we've executed a SUID/SGID binary or a
		 * binary with capabilities.
		 *
		 * For SUID/SGID we just compare euid!=ruid || egid!=rgid.
		 * For capabilities, we currently simply disallow any rld
		 * environment variables.  This probably isn't what we want in
		 * the long haul but it's better to be more restrictive now
		 * until we come up with what we really want.
		 *
		 * If our real UID is root, in some cases we're willing to
		 * allow these environment variables even if we're SUID/SGID
		 * or have capabilities.  If arg1 is 0 then we permit the real
		 * UID=root exception.  If arg1 is not zero then we do *not*
		 * permit the exception.
		 */
		cred_t *cr = get_current_cred();
		rvp->r_val1 = ((cr->cr_uid != cr->cr_ruid ||
				cr->cr_gid != cr->cr_rgid ||
				(cr->cr_cap.cap_effective & CAP_ALL_ON) ||
				(cr->cr_cap.cap_permitted & CAP_ALL_ON)) &&
			       !(cr->cr_ruid == 0 && uap->arg1 == 0));
		return 0;
	}

	case SGI_RECVLUMSG:			/* CIPSO */
        case SGI_GETPSOACL:			/* CIPSO */
        case SGI_SETPSOACL:			/* CIPSO */
		return ENOPKG; 

	case SGI_MPCWAROFF:
		/* no longer needed, but does no harm */
		break;

	case SGI_SET_AUTOPWRON:
		/*
		 * Set system alarm auto power
		 * arg1=time to power back on (assuming that system
		 * shuts down before then!)  Implemented on IP22 and IP26.
		 * Don't sanity check the time, because even if if is later
		 * than the current time, we have no way to be sure we will
		 * be powering down before the time that is set.
		 * if not supported, use ENOPKG.  Don't want to use EINVAL,
		 * since that indicates a bad syssgi command value.
		 */
		if (softpowerup_ok) {
			if (cap_able(CAP_SYSINFO_MGT))
				autopoweron_alarm = uap->arg1;
			else
				error = EPERM;
		}
		else
			error = ENOPKG;		/* not ideal, but close. */
		break;
	case SGI_BTOOLSIZE:
		rvp->r_val1 = btool_size();
		break;
	case SGI_BTOOLGET:
		error = btool_getinfo((void *)uap->arg1);
		break;
	case SGI_BTOOLREINIT:
		btool_reinit();
		break;

	case SGI_NOFPE:
		/*
	 	* disable CSR_EXCEPT within a given range
	 	* from uap->arg1 to uap->arg2.
	 	*/
		CURVPROC_SET_PROXY(VSETP_NOFPE, uap->arg1, uap->arg2);
		break;

	case SGI_GET_UST:
	{
		unsigned long long ust;
		struct timeval tv;
		int s;

		s = splprof();
		update_ust();
		get_ust_nano(&ust);
		if (uap->arg2) microtime(&tv);
		splx(s);
		if (copyout(&ust, (caddr_t)uap->arg1, sizeof (ust)))
			return EFAULT;
		/* 
		 *
		 * if uap->arg2 is NULL, then the code should not attempt
		 * the XLATE_COPYOUT, nor should it return failure.  The
		 * arg2 can be NULL because it is an optional argument to
		 * the syssgi(SGI_GET_UST) call.
		 */
		if (uap->arg2 && XLATE_COPYOUT(&tv, (caddr_t)uap->arg2, 
		    sizeof tv, timeval_to_irix5_xlate, get_current_abi(), 1))
			return EFAULT;

		break;
	}

	case SGI_CREATE_UUID:
	{
		uuid_t	uuid;
		uint_t	status;

		uuid_create (&uuid, &status);
		if (copyout(&status,(caddr_t)uap->arg2,sizeof status))
			return EFAULT;
		if (copyout (&uuid, (caddr_t)uap->arg1,sizeof uuid))
			return EFAULT;
		break;
	}
	
	case SGI_FS_INUMBERS:
	case SGI_FS_BULKSTAT:
		return xfs_itable(uap->cmd, uap->arg1, (void *)uap->arg2,
			uap->arg3, (void *)uap->arg4, (void *)uap->arg5);

	case SGI_FS_BULKSTAT_SINGLE:
		return xfs_itable(uap->cmd, uap->arg1, (void *)uap->arg2,
			1, (void *)uap->arg3, NULL);

	case SGI_FS_SWAPEXT:
		return xfs_swapext((void *)uap->arg1);

	case SGI_PATH_TO_HANDLE:
		return path_to_handle ((char *) uap->arg1,
				(void *) uap->arg2,
				(size_t *) uap->arg3);
	case SGI_PATH_TO_FSHANDLE:
		return path_to_fshandle ((char *) uap->arg1,
				(void *) uap->arg2,
				(size_t *) uap->arg3);
	case SGI_FD_TO_HANDLE:
		return fd_to_handle ((int) uap->arg1,
				(void *) uap->arg2,
				(size_t *) uap->arg3);
	case SGI_OPEN_BY_HANDLE:
		if (!cap_able(CAP_DEVICE_MGT))
			return EPERM;
		return open_by_handle ((void *) uap->arg1,
				(size_t) uap->arg2,
				(int) uap->arg3, rvp);
	case SGI_READLINK_BY_HANDLE:
		if (!cap_able(CAP_DEVICE_MGT))
			return EPERM;
		return readlink_by_handle ((void *) uap->arg1,
				(size_t) uap->arg2,
				(void *) uap->arg3,
				(size_t) uap->arg4, rvp);
	case SGI_ATTR_LIST_BY_HANDLE:
		if (!cap_able(CAP_DEVICE_MGT))
			return EPERM;
		return attr_list_by_handle((void *) uap->arg1,
				(size_t) uap->arg2,
				(void *) uap->arg3,
				(int) uap->arg4,
				(int) uap->arg5,
				(void *) uap->arg6,
				rvp);
	case SGI_ATTR_MULTI_BY_HANDLE:
		if (!cap_able(CAP_DEVICE_MGT))
			return EPERM;
		return	attr_multi_by_handle((void *) uap->arg1,
				(size_t) uap->arg2,
				(void *) uap->arg3,
				(int) uap->arg4,
				(int) uap->arg5,
				rvp);

	case SGI_FSSETDM_BY_HANDLE:
		if (!cap_able(CAP_DEVICE_MGT))
			return EPERM;
		return	fssetdm_by_handle((void *) uap->arg1,
				(size_t) uap->arg2,
				(void *) uap->arg3);
		
	case SGI_XFS_FSOPERATIONS:
		return xfs_fsoperations((int)uap->arg1, (int)uap->arg2, 
				(void *)uap->arg3, (void *)uap->arg4);
			

#if (defined(IP22) || defined(IP28)) && defined(DEBUG)
	case 0xaced:	/* mfg dbg; change power supply voltage */
	{
#include <sys/hpc3.h>
		unsigned bits;
		switch(uap->arg1) {
		case -1:	/* 4.75 */
			bits = MARGIN_LO;
			break;
		case 0:	/* 5.0; nominal */
			bits = 0;
			break;
		case 1:	/* 5.25 */
			bits = MARGIN_HI;
			break;
		default:
			error = EINVAL;
			break;
		}
		if(!error) {
			uint val = _hpc3_write2;
			val &= ~(MARGIN_HI | MARGIN_LO);
			val |= bits;
			_hpc3_write2 = val;
			*(volatile uint *)PHYS_TO_K1( HPC3_WRITE2 ) = _hpc3_write2;
		}
		break;
	}
#endif /* IP22 */

#if (defined(IP19) && defined(_FORCE_ECC))
	case 1001:
	{
		/* InWhat, K1addr, *ecc_data_word struct */

		__psunsigned_t	k1addr = uap->arg2;
		extern _force_ecc(int, __psunsigned_t, __psunsigned_t);

		if (!_CAP_ABLE(CAP_DEVICE_MGT))
			return EPERM;
		if (!ABI_IS_IRIX5_64(get_current_abi()))
			k1addr = PHYS_TO_K1(k1addr & 0x3fffffff);
		
		return _force_ecc(uap->arg1, k1addr, uap->arg3);
	}
#endif	/* IP19 && _FORCE_ECC */

#if defined(IP30) && defined(DEBUG)
	case 0xaced:	/* mfg dbg; change power supply voltage, I2 compt */
	{
		int value;
		if (uap->arg1 == -1)		/* low */
			value = CPU_MARGIN_LO|VTERM_MARGIN_LO|
				PWR_SUPPLY_MARGIN_HI_NORMAL;
	 	else if (uap->arg1 == 0)	/* nominal */
			value = PWR_SUPPLY_MARGIN_LO_NORMAL|
				PWR_SUPPLY_MARGIN_HI_NORMAL;
		else if (uap->arg1 == 1)	/* high */
			value = CPU_MARGIN_HI|VTERM_MARGIN_HI|
				PWR_SUPPLY_MARGIN_LO_NORMAL;
		else if (uap->arg1 & 0x80)	/* 0x80 -> use low bits */
			value = uap->arg1 & 0x3f;

		if (MPCONF->fanloads <= 1)
			value |= FAN_SPEED_LO;
		else if (MPCONF->fanloads >= 1000)
			value |= FAN_SPEED_HI;

		*IP30_VOLTAGE_CTRL = value;

		break;
	}
#endif /* IP30 && DEBUG */
	
#ifdef _MEM_PARITY_WAR
	/* Generate bad parity on IP20/22.
	 * Arg1 is cmd, arg2 points to struct giving addr, pid, and mask.
	 */
	case 1020:
		if(!cap_able(CAP_MEMORY_MGT))
			return EPERM;
		return parity_gun(uap->arg1, (void *)uap->arg2, rvp);
#endif /* _MEM_PARITY_WAR */
	
#if (IP26 || IP28 || IP32) && DEBUG
	/* Generate bad parity on IP20/22.
	 * Arg1 is cmd, arg2 points to struct giving addr, pid, and mask.
	 */
	case 1021:
		if (!cap_able(CAP_MEMORY_MGT))
			return EPERM;
		return ecc_gun(uap->arg1, (void *)uap->arg2, rvp);
#endif /* IP26 || IP28 || IP32 */

	case SGI_CONST:
		switch ((int)uap->arg1) {
		case SGICONST_MBUF:
			return get_mbufconst((void *)uap->arg2,
					(int)uap->arg3,
					(int *)uap->arg4);
		case SGICONST_PTE:
			return get_pteconst((void *)uap->arg2,
					(int)uap->arg3,
					(int *)uap->arg4);
		case SGICONST_PAGESZ:
			return get_pageconst((void *)uap->arg2,
					(int)uap->arg3,
					(int *)uap->arg4);
		case SGICONST_PARAM:
			return get_paramconst((void *)uap->arg2,
					(int)uap->arg3,
					(int *)uap->arg4);
		case SGICONST_SCACHE_SETASSOC:
			return get_scachesetassoc((int *) uap->arg2);
		case SGICONST_SCACHE_SIZE:
			return get_scachesize((int *) uap->arg2);
		}
		return EINVAL;

	case SGI_SHAREII:
		/*
		 *	ShareII system call hook.  If the ShareII system
		 *	is not present, then the process needs to be
		 *	signalled with SIGSYS which nosys() does.
		 */
#ifdef _SHAREII
		return SHR_LIMSYS(uap, rvp);
#else /* _SHAREII */
		return nosys();
#endif /* _SHAREII */

#ifdef	RMAP_DEBUG
	case 1023:
		/* Invoke rmap_scanmap */
		rmap_scantest();
		break;
#endif	/* RMAP_DEBUG */

#ifdef NUMA_MIGR_CONTROL

	case SGI_NUMA_TUNE:
		error = numa_tune((int)uap->arg1, (char*)uap->arg2, (void*)uap->arg3);
		break;

#endif /* NUMA_MIGR_CONTROL */

#ifdef NUMA_BASE

        case SGI_NUMA_STATS_GET:
                error = numa_stats_get((char*)uap->arg1, (void*)uap->arg2);
                break;

#endif /* NUMA_BASE */

#ifdef NUMA_BASE

        case SGI_NUMA_TESTS:
                error = numa_tests((int)uap->arg1,
                                   (cnodeid_t)uap->arg2,
                                   (void*)uap->arg3);
                break;
#endif /* NUMA_BASE */                

#ifdef	DEBUG
	case	SGI_MAPLPAGE:
		error = map_lpage_syscall((size_t)uap->arg1, 
				(caddr_t)uap->arg2, (size_t)uap->arg3, rvp);
		break;
			
	case	SGI_DEBUGLPAGE:
		error = lpage_syscall(uap->arg1, uap->arg2,uap->arg3,rvp);
		break;
#endif

	case SGI_IO_SHOW_AUX_INFO:
	{
		char *info_buf;

#ifdef DEBUG_AUXINFO_CALL
		char buf[512];

		dev_to_name((dev_t) uap->arg1, buf, 512);
		hwgraph_info_get_LBL(hwgraph_path_to_vertex(buf),
				     "sgi_aux_info", 
				     (arbitrary_info_t *)&info_buf);
		printf("%s:%s\n", buf, info_buf);
#endif /* DEBUG_AUXINFO_CALL */

		if (dev_is_vertex((dev_t) uap->arg1)) {
			if (hwgraph_info_get_LBL(dev_to_vhdl
						 ((dev_t) uap->arg1),
						 "sgi_aux_info",
						 (arbitrary_info_t *)
						 &info_buf)
			    == GRAPH_SUCCESS) {
				copyout((void *)info_buf, 
					(void*)uap->arg2, 
					strlen((char *) info_buf));
				error = 0;
			}
			else {
				/* XXX need a new error number */
				error = EINVAL;
			}
		}
		else {
			error = EINVAL;
		}
		break;
	}
 
	case SGI_CACHEFS_SYS:
		error = cachefs_sys((int)uap->arg1, (caddr_t)uap->arg2, rvp);
		break;

       	case SGI_KTHREAD:
	{
		if(!cap_able(CAP_PROC_MGT))
			return EPERM;
		return kthread_syscall (uap->arg1, uap->arg2, rvp);
	}

	    /*KERNEL_ASYNCIO*/

	case SGI_KAIO_READ:
		error = kaio_rw((useraio_t *)uap->arg1, 0);
		break;
	case SGI_KAIO_WRITE:
		error = kaio_rw((useraio_t *)uap->arg1, 1);
		break;
	case SGI_KAIO_SUSPEND:
		error = kaio_suspend((void *)uap->arg1, uap->arg2);
		break;
	case SGI_KAIO_USERINIT:
		error = kaio_user_init((void *)uap->arg1, uap->arg2);
		break;
#if 0
	case SGI_KAIO_STATS:/*use SGI_DBA_GETSTATS(buf,len,-1)*/
		error = kaio_stats((void *)uap->arg1, uap->arg2);
		break;
#endif

	case SGI_DBA_CONFIG:
		return syssgi_dba_conf(uap);
	case SGI_DBA_GETSTATS:
		/* buffer, length, cpu# */
		return dba_getstats((void *)uap->arg1, uap->arg2, uap->arg3);
	case SGI_DBA_CLRSTATS:
		return dba_clearstats();

	case SGI_EVENTCTR:
#ifdef R10000
#if defined(R4000)
		/* the O2 uses a single kernel for R5K and R10K systems */
		return IS_R10000() ? syssgi_eventctr(uap,rvp) : nosys();
#else
		return syssgi_eventctr(uap,rvp);
#endif /* R4000 */
#else
		return nosys();
#endif /* R10000 */

	case SGI_PROCMASK_LOCATION:
		ASSERT(ut == curuthread);
		
		if (uap->arg1 == USER_LEVEL) {
			/*
			 * If we're already at USER_LEVEL, do
			 * nothing.
			 */
			if (ut->ut_sighold != &UT_TO_PROC(ut)->p_sigvec.sv_sighold)
				return 0;
			
			if (error = lockprda(&ut->ut_prda))
				return error;

			uthread_setsigmask(ut, ut->ut_sighold);

		} else {
			/*
			 * For now, we don't handle other args.
			 */
			return EINVAL;
		}
		break;

#ifdef CKPT
	case SGI_CKPT_SYS:
		error = ckpt_sys(uap->arg1, uap->arg2, uap->arg3, uap->arg4,
					uap->arg5, rvp);
		break;
#endif

        case SGI_PMOCTL:
                error = pmoctl((int)uap->arg1, (void*)uap->arg2,
				(void*)uap->arg3, rvp);
                break;

	case SGI_ENUMASHS:
	case SGI_SETASMACHID:
	case SGI_GETASMACHID:
	case SGI_GETARSESS:
	case SGI_JOINARRAYSESS:
		return syssgi_ash(uap,rvp);

	case SGI_ARSESS_CTL:		/* Global array session control */
		return arsess_ctl(uap->arg1,		/* Function code */
				  (void *) uap->arg2,	/* Ptr to arg */
				  uap->arg3);		/* Arg length */

	case SGI_ARSESS_OP:		/* Array session operation */
		return arsess_op(uap->arg1,		/* Function code */
				 (ash_t *) uap->arg2,	/* Ptr to ASH */
				 (void *) uap->arg3,	/* Ptr to arg */
				 uap->arg4);		/* Arg length */

	case SGI_ALLOCSHARENA:
		error = EINVAL;
		break;

	case SGI_DYIELD:
	{
#if USE_PTHREAD_RSA
		extern int dyield(int);

		rvp->r_val1 = dyield((int)uap->arg1);
#else /* !USE_PTHREAD_RSA */
		rvp->r_val1 = 0;
#endif /* !USE_PTHREAD_RSA */
		break;
	}

	case SGI_SETVPID:
	case SGI_GETVPID:
		return EINVAL;

	case SGI_RELEASE_NAME:
		{
		/* release name, human readable, for hardware
		 * specific releases */
		extern char uname_releasename[];

		/* copy null also if room */
		size_t rlen = strlen(uname_releasename)+1;

		if(rlen > (size_t)uap->arg1)
			rlen = (size_t)uap->arg1;
		if (copyout(uname_releasename, (caddr_t)uap->arg2, rlen))
			return EFAULT;
		break;
		}
	case SGI_GET_CONTEXT_NAME:
		{
		    caddr_t kidp = (caddr_t)uap->arg1;
		    caddr_t buf = (caddr_t)uap->arg2;
		    caddr_t lenp = (caddr_t)uap->arg3;
		    char name[PSCOMSIZ];
		    int64_t kid;
		    int len;

		    if (!ABI_HAS_64BIT_REGS(abi))
			return EINVAL;
		    if (copyin(lenp, &len, sizeof(int)))
			return EFAULT;
		    if (copyin(kidp, &kid, sizeof(int64_t)))
			return EFAULT;
		    /*
		     * Find the proc, sthread,  or xthread 
		     * corresponding to kid and copy out its name.
		     */
		    {
			    struct kiu_arg arg;
			    
			    arg.kid = kid;
			    arg.pid = 0;
			    procscan(kid_is_uthread, &arg);
			    if (arg.pid) {
				    size_t namelen = strlen(arg.name);
				    if (len > namelen)
					    len = namelen;
				    if (len)
					    if(copyout(arg.name, buf, len))
						    return EFAULT;
				    if (copyout(&len, lenp, sizeof(len)))
					    return EFAULT;
				    rvp->r_val1 = arg.pid;
				    return 0;
			    }
			    
			    
		    }
		    {
			extern sthread_t sthreadlist;
			extern lock_t st_list_lock;
			sthread_t *st;
			int s = mutex_spinlock(&st_list_lock);

			for (st = sthreadlist.st_next;
			     st != &sthreadlist;
			     st = st->st_next)
			    if (ST_TO_KT(st)->k_id == kid) {
				size_t namelen = (st->st_name[ST_NAMELEN-1]
						  ? ST_NAMELEN
						  : strlen(st->st_name));
				if (len > namelen)
				    len = namelen;
				strncpy(name,st->st_name, len); 
				mutex_spinunlock(&st_list_lock, s);
				if (len)
					error = copyout(name, buf, len);
				else
					error = 0;
				if (copyout(&len, lenp, sizeof(len)))
					return EFAULT;	
				rvp->r_val1 = 0;
				return error;
			    }
			mutex_spinunlock(&st_list_lock, s);
		    }
		    {
			extern xthread_t xthreadlist;
			extern lock_t xt_list_lock;
			xthread_t *xt;
			int s = mutex_spinlock(&xt_list_lock);

			for (xt = xthreadlist.xt_next;
			     xt != &xthreadlist;
			     xt = xt->xt_next)
			    if (XT_TO_KT(xt)->k_id == kid) {
				size_t namelen = (xt->xt_name[IT_NAMELEN-1]
						  ? IT_NAMELEN
						  : strlen(xt->xt_name));
				if (len > namelen)
				    len = namelen;
				strncpy(name, xt->xt_name, len);
				mutex_spinunlock(&xt_list_lock, s);
				if (len)
					error = copyout(name, buf, len);
				else
					error = 0;
				if (copyout(&len, lenp, sizeof(len)))
					return EFAULT;	
				rvp->r_val1 = 0;
				return error;
			    }
			mutex_spinunlock(&xt_list_lock, s);
		    }
		}
		return ESRCH;

	case SGI_GET_CONTEXT_INFO:
		/* reserved but not implemented */
                return ENOSYS;

#if defined(SN0)
        case SGI_PART_OPERATIONS:
		return part_operation((int)uap->arg1, (int)uap->arg2, 
				      (void *)uap->arg3, (void *)uap->arg4, rvp);
#endif

#ifdef SW_FAST_CACHE_SYNCH
	case SGI_SYNCH_CACHE_HANDLER:
		{
		extern int cachesynch(caddr_t, uint);
		extern int cache_sync_bp_start[], cache_sync_bp_end[];
		int rlen = (cache_sync_bp_end - cache_sync_bp_start) * sizeof(inst_t);
		if(rlen > uap->arg2)
			return EFAULT;
		if (copyout((caddr_t)cache_sync_bp_start, (caddr_t)uap->arg1, rlen))
			return EFAULT;

		if (IS_MP) {
			/* The fast-path cache flush code doesn't deal with 
			 * interprocessor coherency.  If swin happens to get 
			 * rescheduled to a different processor at an 
			 * inopportune time, this could cause it to execute
			 * garbage.  To prevent this, we pin the process to
			 * the current processor.  An alternative would be
			 * to have a callback facility in the scheduler
			 * so we could flush the cache when we change 
			 * processors.
			 */
			mp_mustrun(0, cpuid(), 0, 1);
		}

		/* synchronize buffer */
		error = cachesynch((caddr_t)uap->arg1, rlen);
		break;
		}
#endif
#ifdef DEBUG
	case SGI_MUTEX_TEST:
		rvp->r_val1 = mutex_test(uap->arg1);
		break;
	case SGI_MUTEX_TEST_INIT:
		mutex_test_init();
		break;
	case SGI_MUTEX_TESTER_INIT:
		mutex_tester_init();
		break;
#endif
	case SGI_CONTEXT_SWITCH:
		sv_context_switch(uap->arg1);
		break;

	case SGI_MRLOCK_TEST_INIT:
		mri_test_init();
		break;
	case SGI_MRLOCK_TEST_RLOCK:
		mri_test_dolock(uap->arg1, uap->arg2);
		break;

#if defined (SN0) && defined (FORCE_ERRORS)
	case SGI_ERROR_FORCE:
		error = error_induce(&uap->arg1, rvp);
		break;
#else
	case SGI_ERROR_FORCE:
		error = nosys();
		break;
#endif

#ifdef DATAPIPE
	case SGI_DPIPE_FSPE_BIND:
		error = fspe_bind((caddr_t)uap->arg1);
		break;
#endif /* DATAPIPE */

	case SGI_IS_DEBUG_KERNEL:
#if DEBUG
		rvp->r_val1 = 1;
#else
		rvp->r_val1 = 0;
#endif
		break;

	case SGI_IS_TRAPLOG_DEBUG_KERNEL:
#if TRAPLOG_DEBUG && (_PAGESZ == 16384)
		rvp->r_val1 = 1;
#else
		rvp->r_val1 = 0;
#endif
		break;

#if defined(DEBUG) || defined(POKEPEEK)
	case SGI_POKE:
	{
	    uint64_t x;
	    if (!cap_able(CAP_MEMORY_MGT))
		return EPERM;
	    if (copyin((caddr_t)uap->arg3, &x, sizeof(x)))
		return EFAULT;
	    switch (uap->arg1) {
	    case 1:
		*(uint8_t *) uap->arg2 = (uint8_t) x;
		break;
	    case 2:
		*(uint16_t *) uap->arg2 = (uint16_t) x;
		break;
	    case 4:
		*(uint32_t *) uap->arg2 = (uint32_t) x;
		break;
	    case 8:
		*(uint64_t *) uap->arg2 = (uint64_t) x;
		break;
	    }
	    break;
	}

	case SGI_PEEK:
	{
	    uint64_t x;
	    if (!cap_able(CAP_MEMORY_MGT))
		return EPERM;
	    switch (uap->arg1) {
	    case 1:
		x = *(uint8_t *) uap->arg2;
		break;
	    case 2:
		x = *(uint16_t *) uap->arg2;
		break;
	    case 4:
		x = *(uint32_t *) uap->arg2;
		break;
	    case 8:
		x = *(uint64_t *) uap->arg2;
		break;
	    }
	    if (copyout(&x, (caddr_t) uap->arg3, sizeof(x)))
		return EFAULT;
	    break;
	}
#endif

	case SGI_FO_DUMP:
		{
		struct user_fo_instance *buf, *u_foi;
		int                      alloc_len, len;
		struct scsi_candidate   *foc;
		struct scsi_fo_instance *foi;
		int                      i;

		if (copyin((caddr_t) uap->arg1, &alloc_len, sizeof(int))) 
		    return EFAULT;

		if (alloc_len < sizeof(struct user_fo_instance))
			return EINVAL; 

		if ((buf = kmem_zalloc(alloc_len, KM_NOSLEEP)) == NULL)
		    return ENOMEM;

		u_foi = buf;
		len = 0;
		for (foc = fo_candidates; foc->scsi_match_str1; ++foc) {
		    for (foi = foc->scsi_foi; foi; foi = foi->foi_next) {

		      len += sizeof(struct user_fo_instance);
		      if (len > alloc_len) {
			 error = ENOMEM;
			 goto fo_dump_done;
		      }
		      u_foi->foi_primary = foi->foi_primary;
		      for (i = 0; i < MAX_FO_PATHS; ++i)
			u_foi->foi_path_vhdl[i] = foi->foi_path[i].foi_path_vhdl;
		      ++u_foi;
		    }
		}

		if (copyout((caddr_t) buf, (caddr_t) uap->arg2, len)) {
		    error = EFAULT;
		    goto fo_dump_done;
		}
		if (copyout((caddr_t) &len, (caddr_t) uap->arg1, sizeof(int)))
		{
		    error = EFAULT;
		    goto fo_dump_done;
		}
	fo_dump_done:
		kmem_free(buf, alloc_len);
		break;
		}

	case SGI_FO_SWITCH:
	case SGI_FO_TRESSPASS:
	{
		vertex_hdl_t p_vhdl, s_vhdl;
		int          rc;

		if (!cap_able(CAP_DEVICE_MGT))
		      return EPERM;

		p_vhdl = (vertex_hdl_t)uap->arg1;
#if DEBUG
		printf("syssgi: SGI_FO_SWITCH p_vhdl = %d\n", p_vhdl);
#endif
		if (uap->cmd == SGI_FO_TRESSPASS)
			rc = fo_scsi_device_switch_new(p_vhdl, &s_vhdl, 1);
		else
			rc = fo_scsi_device_switch_new(p_vhdl, &s_vhdl, 0);
#if DEBUG
		printf("syssgi: SGI_FO_SWITCH s_vhdl = %d\n", s_vhdl);
#endif
		if (rc == FO_SWITCH_SUCCESS || rc == FO_SWITCH_ONEPATHONLY) {
			/* copyout if s_vhdl returned */
			if (copyout((caddr_t) &s_vhdl, (caddr_t) uap->arg2, sizeof(vertex_hdl_t)))
				return(EFAULT);
		}

		if (rc != FO_SWITCH_SUCCESS) 
		  return(EIO);
	}
		break;
#ifdef	DEBUG
	case SGI_SHAKE_ZONES:
	{
		extern void kmem_test_shake(int, int);
		kmem_test_shake(uap->arg1, uap->arg2);
		break;
	}

	case SGI_KMEM_TEST:
	{
		extern void kmem_test(int, int);
		kmem_test(uap->arg1, uap->arg2);
		break;
	}
#endif



	case SGI_CELL:
	{
	     /*
	      * Usage:  syssgi(SGI_CELL, SGI_CELL_PID_TO_CELLID, pid, &cellid)
	      *
	      * Note that for non-cell systems, the cell id returned is 0.
	      */
	     switch ((int)uap->arg1) {
		  
	     case SGI_IS_OS_CELLULAR:
#ifdef CELL
		  return 0;
#else
		  return 1;
#endif /*cell*/
		  
	     case	SGI_CELL_PID_TO_CELLID:
	     {
		  vproc_t 		*vpr;
			pid_t		pid;
			vp_get_attr_t	attr;

			pid = uap->arg2;
			if (pid < 0 || pid >= MAXPID)
			    return EINVAL;

			if (pid == 0)
			    pid = current_pid();

			if (pid == current_pid())
			    vpr = curvprocp;
			else if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			    return ESRCH;

			VPROC_GET_ATTR(vpr, VGATTR_CELLID, &attr);

			if (pid != current_pid())
			     VPROC_RELE(vpr);
			
			if (suhalf((void *)uap->arg3, attr.va_cellid))
			     return EFAULT;
			break;
			
	     }
	     
#ifdef CELL
		case SGI_MESG_STATS:
		{
			extern int export_message_stat( struct syssgia *uap);
			int ret_val;
			ret_val  = export_message_stat(uap);
			if(ret_val)
				return ret_val; 
			else
				break;
		}
	     
		case SGI_MEMBERSHIP_STATS:
		{
			extern int export_membership_stat(struct syssgia *uap);
			int ret_val;

			ret_val = export_membership_stat(uap);
			if(ret_val)
				return ret_val;
			else
				break;
		}
#endif /*CELL*/

#ifdef CELL_IRIX
		case	SGI_CELL_OBJ_EVICT:
		{
			service_t	svc;
			service_t	srv_svc;
			void	*id;
			cell_t	target;

			if (!cap_able(CAP_SYSINFO_MGT))
			    return EPERM;

			SERVICE_MAKE(svc, cellid(), uap->arg2);
			id = (void *) uap->arg3;	/* object id */
			target = uap->arg4;		/* target cell */

			if (id == NULL) {
			    /* Evict all local objects */
			    error = obj_svr_evict(svc, id, target);
			} else {
			    /* Evict one object - looked-up by id */
			    error = obj_svr_lookup(svc, id, &srv_svc);
			    if (!error) {
				    error = obj_svr_evict(srv_svc, id, target);
			    }
			}
			break;
		}
#endif /* CELL*/
#if	defined(CELL) && defined(DEBUG)
		case SGI_LEAVE_MEMBERSHIP:
			{
				extern	void 	cms_leave_membership(void);
				cms_leave_membership();
				break;
			}
		case SGI_SEND_TEST_MESG:
			{
				extern void 	cell_test_mesg(cell_t);
				cell_test_mesg((cell_t)uap->arg1);
				break;
			}
		case SGI_FAIL_CELL:
			{
				/*
				 * Clear the sanity check variable.
				 */
				extern void	cell_suicide(void);
				cell_suicide();
				break;
			}
#endif
		default:
	            return EINVAL;
	    }

	    break;
	}

        
#if defined(INDUCE_IO_ERROR)
	
	case SGI_XLV_INDUCE_IO_ERROR:
		xlv_insert_error = 1;
		xlv_error_dev2 = (dev_t)uap->arg1;
		xlv_error_freq = (int)uap->arg2;
		break;	
	case SGI_XLV_UNINDUCE_IO_ERROR:
		xlv_insert_error = 0;
		xlv_error_dev2 = 0;
		xlv_error_freq = 0;
		break;	
	case SGI_DKSC_INDUCE_IO_ERROR:
		dksc_insert_error = 1;
		dksc_error_dev = (dev_t)uap->arg1;
		dksc_error_freq = (int)uap->arg2;
		break;	
	case SGI_DKSC_UNINDUCE_IO_ERROR:
		dksc_insert_error = 0;
		dksc_error_dev = 0;
		dksc_error_freq = 0;
		break;		
#else
	
	case SGI_XLV_INDUCE_IO_ERROR:
	case SGI_XLV_UNINDUCE_IO_ERROR:
	case SGI_DKSC_INDUCE_IO_ERROR:
	case SGI_DKSC_UNINDUCE_IO_ERROR:
		error = nosys();
		break;	
#endif

#if defined(DEBUG) || defined(INDUCE_IO_ERROR)
	case SGI_XFS_INJECT_ERROR:
		rvp->r_val1 = xfs_errortag_add((int) uap->arg1,
					       (int) uap->arg2);
		break;
	case SGI_XFS_CLEAR_ERROR:
		rvp->r_val1 = xfs_errortag_clear((int) uap->arg1,
						(int) uap->arg2);
		break;
	case SGI_XFS_CLEARALL_ERROR:
		xfs_errortag_clearall((int) uap->arg1);
		break;
#else
	case SGI_XFS_INJECT_ERROR:
	case SGI_XFS_CLEAR_ERROR:
	case SGI_XFS_CLEARALL_ERROR:
		error = nosys();
		break;
#endif /* DEBUG || INDUCE_IO_ERROR */

	case SGI_XFS_MAKE_SHARED_RO:
		error = xfs_mk_sharedro((int) uap->arg1);
		break;

	case SGI_XFS_CLEAR_SHARED_RO:
		error = xfs_clear_sharedro((int) uap->arg1);
		break;

	case SGI_EARLY_ADD_SWAP:
	{
		extern void sd_init(int);
		if (!cap_able(CAP_DEVICE_MGT)) {
			rvp->r_val1 = -1;
			return EPERM;
		}
		sd_init(0);
		break;
	}

	case SGI_NOHANG:
		rvp->r_val1 =
			curuthread->ut_pproxy->prxy_flags & PRXY_NOHANG ? 1 : 0;
		if (uap->arg1) {
			prxy_flagset(curuthread->ut_pproxy, PRXY_NOHANG);
#ifdef DEBUG_NOHANG			
			if (uap->arg1 == 1) {
				prxy_flagclr(curuthread->ut_pproxy, PRXY_RLCKDEBUG);
				prxy_flagclr(curuthread->ut_pproxy, PRXY_RWLCKDEBUG);
			} else if (uap->arg1 == 2) {
				prxy_flagset(curuthread->ut_pproxy, PRXY_RLCKDEBUG);
				prxy_flagclr(curuthread->ut_pproxy, PRXY_RWLCKDEBUG);
			} else if (uap->arg1 == 3) {
				prxy_flagclr(curuthread->ut_pproxy, PRXY_RLCKDEBUG);
				prxy_flagset(curuthread->ut_pproxy, PRXY_RWLCKDEBUG);
			} else if (uap->arg1 == 4) {
				prxy_flagset(curuthread->ut_pproxy, PRXY_RLCKDEBUG);
				prxy_flagset(curuthread->ut_pproxy, PRXY_RWLCKDEBUG);
			}
#endif
		} else {
			prxy_flagclr(curuthread->ut_pproxy, PRXY_NOHANG);
		}
		break;

	case SGI_UNFS: {
		extern	void stop_nfs(void *);

		VPROC_ADD_EXIT_CALLBACK(curvprocp, ADDEXIT_CHECK_DUPS, 
		    stop_nfs, (void*)0, error);
		break;
	    }
	
	case SGI_UNICENTER: {
		error = caienfk((void*)uap->arg1, rvp);
		break;
	    }

	case SGI_IPC_AUTORMID_SHM: {
		rvp->r_val1 = IPC_AUTORMID;
		break;
	    }


	/*
	 * This is an undocumented interface for diagnostics to use
	 * to flush the local icache. This interface is intended to
	 * be used on R10k chips or chips that have inclusive icache
	 * policies, ie. icache is a subset of scache.
	 * (On platforms without inclusive cache, diagnostics may pin
	 * themselves to a cpu - this willl guarantee correct
	 * functionality).
	 *
	 *	syssgi(SGI_FLUSH_ICACHE, addr, bytecnt)
	 *
	 * Notes: 
	 *	- not supported on IP21, IP25 or IP26
	 *		(could be made to work on these but no requirement
	 *		 right now)
	 *	- uses rt_pin_thread to keep thread from changing cpus
	 *	- get exclusive ownership of all cache lines in range. This
	 *	  purges other cpu's ICACHE since icache is inclusive 
	 *	  (per request interface - only works if inclusive)
	 *	- purges local icache
	 *	- unpins thread
	 */
	case SGI_FLUSH_ICACHE: {
#if IP21 || IP25 || IP26
		/*
		 * Some platform have wrong cache attributes and/or dont
		 * support the required icache flush functions.
		 */
		return(ENOTSUP);
#else
		int	b;
		char	*vaddr, *vaddrend, *cookie;

		if (uap->arg2 <= 0)
			return(EINVAL);

		vaddr = (char*)uap->arg1;
		vaddrend = (char*)(uap->arg1 + uap->arg2 - 1);	/* last byte to touch */
		cookie = rt_pin_thread();
		/*
		 * Touch at least 1 byte in each cache line in the <addr> to <addr+len-1>
		 * range specified for the cache flush.
		 */
		while(1) {
			if ( ((b=fubyte(vaddr)) == -1) || subyte(vaddr, b) == -1) {
				rt_unpin_thread(cookie);
				return(EFAULT);
			}
			if (vaddr == vaddrend) 
				break;
			vaddr += (scache_linemask + 1);
			if (vaddr > vaddrend)
				vaddr = vaddrend;
		}

		__icache_inval((void*)uap->arg1, uap->arg2);
		rt_unpin_thread(cookie);
		break;
#endif
	    }


	/*
	 * this is an undocumented interface we used for TREX HW checkout
	 *
	 * since it is handy to check out the CPU configurations on a 
	 * system, we'll make this part of syssgi().
	 *
	 * Two possible invocations :
	 *
	 *	- syssgi(SGI_HW_CPU_CONFREG,cpu,&reg,NULL)
	 *
	 *		to read the CPU register
	 *
	 *	  returns 1 if call succeeds (one argument copied out)
	 *
	 *	- syssgi(SGI_HW_CPU_CONFREG,cpu,&reg,&ip27config)
	 *
	 *		can be used on SN0 to read out PROM information
	 *		which contains for example the tap setting which
	 *		is not available via the CPU register
	 *
	 *	  return 2 if calls succeeds (two arguments copied out)
	 */

#if defined(SN0) 	

	case SGI_HW_CPU_CONFREG : {

#include <sys/SN/SN0/ip27config.h>

                cpu_cookie_t cookie;
		cpuid_t      cpu = uap->arg1;

                if (cpu >= 0 && cpu <= maxcpus && cpu_enabled(uap->arg1)) {
                        int get_cpu_cnf (void);
                        int val;
			cnodeid_t      node;
			nasid_t        nasid;
			ip27config_t *ip27config;

                        cookie = setmustrun(uap->arg1);
                        val    = get_cpu_cnf();
			restoremustrun(cookie);

                        if (copyout(&val, (int *) uap->arg2, 4) < 0) {
                                error = EFAULT;
				rvp->r_val1 = -1;	

				break;
				/*NOTREACHED*/
							
			} else {
				rvp->r_val1 = 1;	/* one argument */
							/* copied out   */
			}


			if (uap->arg3 != NULL) {
                        	node  = CPUID_TO_COMPACT_NODEID(uap->arg1);
                        	nasid = COMPACT_TO_NASID_NODEID(node);
                        	ip27config = (ip27config_t *)
                                        IP27CONFIG_ADDR_NODE(nasid);

                        	if (copyout(ip27config,(int *) uap->arg3,
                                		sizeof(ip27config_t)) < 0) {

                                	error = EFAULT;
				} else {
				    	rvp->r_val1 = 2;/* two arguments   */
							/*  copied out     */
				}
			 }
		} else {
			error       = ENODEV;
			rvp->r_val1 = -1;
		}

		break;
	}
#endif

	/*
	 * upanic support
	 *
	 * used to trigger a system panic in a test environment. 
	 *
	 * A system tuneable "enable_upanic" must be set to enable
	 * this feature.
	 *
	 * A priviledged user then can set the upanic flag, allowing
	 * any other user to provoke a system panic for debugging.
	 *
	 * syssgi(SGI_UPANIC_SET,-1)	reports the current setting
	 * syssgi(SGI_UPANIC_SET, 0)	disables upanic
	 * syssgi(SGI_UPANIC_SET, 1)	enables  upanic
	 */ 
	case SGI_UPANIC_SET: {
		extern int enable_upanic;

		if (!cap_able(CAP_DEVICE_MGT))
                        return EPERM;

		/*
		 * if upanic has not been turned on explictly
		 * via the system tuneable return error
		 */
		if (! enable_upanic)
			return EACCES;

		switch (uap->arg1) {
		case -1 : 
			rvp->r_val1 = upanic_flag; 
			break;
		case  0 : 
			if (upanic_flag == 1) {
				upanic_flag = 0;
				cmn_err_tag(1767, CE_WARN,
					"disabling user callable panic\n");
			}
			break;
		case  1 : 
			if (upanic_flag == 0) {
				upanic_flag = 1; 
				cmn_err_tag(1768, CE_WARN,
					"enabling user callable panic\n");
			}
			break;
		default : 
			return EINVAL;
			/*NOTREACHED*/
		}
		break;
	}

	case SGI_UPANIC : {
		extern int enable_upanic;

		if (! enable_upanic)
			return EACCES;

		if (upanic_flag == 0)
			return EPERM;

		cmn_err_tag(1769, CE_PANIC,"user requested system panic\n");

		break;
	}

	case SGI_NFS_UNMNT: {
		dev_t dev = uap->arg1;
		extern int umount_dev(dev_t dev);
		
		error = umount_dev(dev);

		break;
	}

	default:
		error = EINVAL;
		break;
	}
    
	return error;
}


static int
syssgi_hostident(struct syssgia *uap)
{
	char	hostident[ MAXSYSIDSIZE ];
	int 	error = 0;

	switch (uap->cmd) {
	case SGI_SYSID:
		/*
		 * SYSTEM ID
		 * determined in a 'machine dependant' way.
		 */
		if (error = getsysid(hostident))
			return error;
		if (copyout(hostident, (caddr_t)uap->arg1, MAXSYSIDSIZE))
			return EFAULT;
		break;

	case SGI_MODULE_INFO:
	{
#ifdef SN0
		module_info_t 	m_info;
		  
		if (error = get_kmod_info(uap->arg1,&m_info))
			return error;
			  			  
		if (copyout(&m_info,(module_info_t*)uap->arg2,
			      min(sizeof(module_info_t),uap->arg3)))
			return EFAULT;
#else /* !SN0 */
		module_info_t 	m_info;
		int        		s_number;
		  
		if (uap->arg1 != 0) {
			  /* All platforms besides SN0 only have module 0 */
			return EINVAL;
		}
		else {
#ifndef EVEREST
			int i;
			char *cur;
#endif
			char serial_str[MAXSYSIDSIZE];

			if (error = get_host_id(hostident,&s_number,
						serial_str))
				return error;
			  
			m_info.serial_num = s_number;
	 		m_info.mod_num = 0;

#ifdef EVEREST
			strncpy(m_info.serial_str, serial_str,
				MAX_SERIAL_SIZE);
#else /* !EVEREST */
			  
#ifdef IP20 /* if we are an IP20 we can't get the real serial number so leave
	       the string blank */
			m_info.serial_str[0] = '\0';
			  
#else /* !IP20 */

			/* convert to 12 hex characters */
			m_info.serial_str[0] = '0';
			m_info.serial_str[1] = '8';
			m_info.serial_str[2] = '0';
			m_info.serial_str[3] = '0';
			m_info.serial_str[12] = '\0';
			cur = &m_info.serial_str[11];
			for (i = 0; i < 8; i++) {
				if (s_number) {
					*cur-- = "0123456789ABCDEF"[s_number%16];
					s_number /= 16;
				}
				else
					*cur-- = '0';
			}
#endif /* !IP20 */
#endif /* !EVEREST */
			  
			if (copyout(&m_info,(module_info_t*)uap->arg2,
				     min(sizeof(module_info_t),uap->arg3)))
				return EFAULT;
		}
#endif /* !SN0 */
		break;
	}
	}
	return error;
}


static int
syssgi_tune(struct syssgia *uap)
{
	int error = 0;
#if _MIPS_SIM == _ABI64
	int abi = get_current_abi();
#endif

	switch (uap->cmd) {
	case SGI_TUNE_SET:
	{
		char groupname[GROUPNAMESZ+1];
		char tname[TUNENAMESZ+1];
		struct tunename *tn;
		char tmp_value[sizeof(uint64_t)];
		uint64_t value;
		int  g_index, t_index, i_value;
		typedef int (*tsan_t)(int *, int);

#if _MIPS_SIM == _ABI64
		if (!ABI_IS_IRIX5_64(abi))
			return EINVAL;
#endif

		if (!cap_able(CAP_SYSINFO_MGT))
			return EPERM;
		if (copyin((caddr_t)uap->arg1, &groupname[0], GROUPNAMESZ+1))
			return EFAULT;
		if (copyin((caddr_t)uap->arg2, &tname[0], TUNENAMESZ+1))
			return EFAULT;

		g_index = tunetablefind(&groupname[0]);
		if (g_index < 0)
			return EINVAL;

		for (tn = tunename, t_index = 0; tn->t_addr; tn++, t_index++) {
			if (!strcmp(tname, tn->t_name)) {
				break;
			}
		}
		if (!tn->t_addr || tn->t_group != g_index)
			return EINVAL;


		/* Since the specified value can be a pointer to either
		 * a 64-bit or 32-bit value we may need to convert it.
		 */

		if (copyin((caddr_t)uap->arg3, &tmp_value, tn->t_size))
			return EFAULT;

		if (tn->t_size == sizeof(uint64_t)) {
			value = *(uint64_t *)tmp_value;
		}
		else {
			i_value = *(int *)tmp_value;
			value = (uint64_t)i_value;
		}

		VHOST_SYSTUNE(g_index, t_index, value, error);
		if (error) {
			return error;
		}
		break;
	}

	case SGI_TUNE_GET:
	{
		char tname[TUNENAMESZ+1];
		struct tunename *tn;
		extern struct tunename tunename[];

#if _MIPS_SIM == _ABI64
		if (!ABI_IS_IRIX5_64(abi))
			return EINVAL;
#endif

		if (copyin((caddr_t)uap->arg1, &tname[0], TUNENAMESZ+1))
			return EFAULT;

		for (tn = tunename; tn->t_addr; tn++) {
			if (!strcmp(tname, tn->t_name)) {
				break;
			}
		}
		if (!tn->t_addr) 
			return EINVAL;

		if (copyout((caddr_t)tn->t_addr, (caddr_t)uap->arg2,
				 tn->t_size))
			return EFAULT;
		break;
	}
	}
	return error;
}


static int
syssgi_nvram(struct syssgia *uap)
{
	switch (uap->cmd) {

	/*
	 * Set nvram variable with a given value.
	 *
	 * arg1 -- nvram var name.
	 * arg2 -- nvram var value.
	 * arg3 -- not used.
	 */
	case SGI_SETNVRAM:
	{
		int rc;

		if (!cap_able(CAP_SYSINFO_MGT))
			return EPERM;

		if (copyin((caddr_t)uap->arg1, &nv_name[0], SGI_NVSTRSIZE))
			return EFAULT;

		if (copyin((caddr_t)uap->arg2, &nv_val[0], SGI_NVSTRSIZE))
			return EFAULT;

#ifdef EVEREST
		if (strcmp(nv_name, "uptime") == 0) {
			if (strcmp(nv_val, "record") == 0)
				record_uptime = 1;
			else if (strcmp(nv_val, "unrecord") == 0)
				record_uptime = 0;
			else return EINVAL;
			return 0;
		}

		if(strcmp(nv_name,"fru") == 0){
		  set_fru_nvram(nv_val,strlen(nv_val));
		  return 0;
		}
#endif

#ifdef SN0
		if (strcmp(nv_name, "PartReboot") == 0) {
			set_part_reboot_nvram(nv_val) ;
			return 0 ;
		}
#endif
		if (rc = set_nvram(&nv_name[0], &nv_val[0]))
			return (rc == -2) ? ENXIO : EINVAL;

		if (kopt_set(&nv_name[0], &nv_val[0]))
			return EINVAL;

		break;
	}

	/*
	 * set a given variable in the kernel environment
	 * called the same way/works the same as SGI_SETNVRAM,
	 * except we assume the nvram set happened outside
	 * of the kernel.
	 */
	case SGI_SETKOPT:
	{

		if (!cap_able(CAP_SYSINFO_MGT))
			return EPERM;

		if (copyin((caddr_t)uap->arg1, &nv_name[0], SGI_NVSTRSIZE))
			return EFAULT;

		if (copyin((caddr_t)uap->arg2, &nv_val[0], SGI_NVSTRSIZE))
			return EFAULT;

		if (kopt_set(&nv_name[0], &nv_val[0]))
			return EINVAL;

		break;
	}

	/*
	 * Read nvram variable in a given buf.
	 *
	 * arg1 -- nvram var name.
	 * arg2 -- buffer ptr.
	 * arg3 -- not used.
	 */
	case SGI_GETNVRAM:
	{
		char *cp;
		char *np, *vp;
		int nv_idx;

		if (copyin((caddr_t)uap->arg1, &nv_name[0], SGI_NVSTRSIZE))
			return EFAULT;

#ifdef EVEREST
		if (strcmp(nv_name, "uptime")==0){
		  char value[5],tmp[32];
		  
		  /* get the value in powerfail nvram register */
		  get_pwr_fail(value);
		  bzero(tmp, 32);
		  sprintf(tmp, "%u", * (uint *) value);
		  if (copyout(tmp, (caddr_t)uap->arg2, (strlen(tmp) + 1)))
		    return EFAULT;
		  else
		    return 0;
		}

		if (strcmp(nv_name, "fru")==0){
		  /* Get the value stored in fru nvram register */
		  get_fru_nvram(nv_val);
		  if(copyout(nv_val,(caddr_t)uap->arg2, (strlen(nv_val) + 1)))
		    return EFAULT;
		  else
		    return 0;
		}
#endif /* EVEREST */

		if ((nv_name[0] >= '0') && (nv_name[0] <= '9')) {
			nv_idx = atoi(nv_name);
			if (kopt_index(nv_idx, &np, &vp))
				return EINVAL;

			if (copyout(np, (caddr_t)uap->arg1,
				    min((strlen(np) + 1), SGI_NVSTRSIZE)))
				return EFAULT;

			if (copyout(vp, (caddr_t)uap->arg2,
				    min((strlen(vp) + 1), SGI_NVSTRSIZE)))
				return EFAULT;
		} else {
		        if (!(cp = kopt_find(nv_name)))
			  return EINVAL;

			if (copyout(cp, (caddr_t)uap->arg2,
				    min((strlen(cp) + 1), SGI_NVSTRSIZE)))
				return EFAULT;
		}

		break;
	}
	}
	return 0;
}


static int
syssgi_dba_conf(struct syssgia *uap)
{
	int error = 0;

	/* database accelerator configuration */
	struct dba_conf dbaconf;

	switch(uap->arg1) {
	case SGI_DBCF_GET:
		if (error = dba_getconfig(&dbaconf))
			return error;
		dbaconf.dbcf_sizeused = MIN(sizeof(struct dba_conf), uap->arg3);
		if (copyout((void *)&dbaconf,
			    (void *)uap->arg2,
			    dbaconf.dbcf_sizeused))
			return EFAULT;
		break;
	case SGI_DBCF_PUT:
		dbaconf.dbcf_sizeused = MIN(sizeof(struct dba_conf), uap->arg3);
		if (copyin((void *)uap->arg2,
			   (void *)&dbaconf,
			   dbaconf.dbcf_sizeused))
			return EFAULT;
		if (error = dba_putconfig(&dbaconf))
			return error;
		break;
	default:
		return nosys();
	}
	return 0;
}


#ifdef R10000
static int
syssgi_eventctr(struct syssgia *uap, rval_t *rvp)
{
	uthread_t *ut = curuthread;
	int error = 0;

	switch (uap->arg1) {

	case HWPERF_PROFENABLE:
	{
		/*
		 * Acquire and start using the counters.	
		 * This call is for the profiling usage of 
		 * the counters, and so the user has specified
		 * overflow frequencies for the relevant events.
		 */
		
		hwperf_profevctrarg_t 	enargs;
		int			rval;		

		if (copyin((void *)uap->arg2, 
				(void *)&enargs,
				sizeof(hwperf_profevctrarg_t)))
			return EFAULT;
		error = evc_profil(&enargs, (void *)uap->arg3,
				  uap->arg4, uap->arg5, &rval);
		rvp->r_val1 = rval;
		return error;
	}
	case HWPERF_ENSYSCNTRS:
	{
		
		hwperf_profevctrarg_t 	enargs;
		int			rval;		

		if (!cap_able(CAP_DEVICE_MGT)) {
			rvp->r_val1 = -1;
			return EPERM;
		}
		if (copyin((void *)uap->arg2, 
				(void *)&enargs,
				sizeof(hwperf_profevctrarg_t))) {
			rvp->r_val1 = -1;
			return EFAULT;
		}
		/*
		 * hwperf_enable_sys_counters() does not return
		 * until the caller is the sole owner of the
		 * counters in system mode, which means that it must
		 * force others off.
		 */
		error = hwperf_enable_sys_counters(&enargs, NULL,
						   0, &rval);
		if (error)
			return error;
		rvp->r_val1 = rval;
		return 0;
	}
	case HWPERF_PROF_CTR_SUSP:
	{
		/*
		 * Short circuit this call until we figure out how we
		 * want to fix bug #502094, ``assert being taken in
		 * r10kperf_stop_counters.
		 */
		return EINVAL;
		/*NOTREACHED*/

		if (ut->ut_cpumon &&
		    ut->ut_cpumon->cm_flags & HWPERF_CM_ENABLED) {
			SUSPEND_HWPERF_COUNTERS(ut);
			ut->ut_cpumon->cm_flags &= ~HWPERF_CM_ENABLED;
			ut->ut_cpumon->cm_flags |= HWPERF_CM_SUSPENDED;
		}
		return 0;
	}
	case HWPERF_PROF_CTR_CONT:
	{
		/*
		 * Short circuit this call until we figure out how we
		 * want to fix bug #502094, ``assert being taken in
		 * r10kperf_stop_counters.
		 */
		return EINVAL;
		/*NOTREACHED*/

		if (ut->ut_cpumon &&
		    ut->ut_cpumon->cm_flags & HWPERF_CM_ENABLED) {
			ut->ut_cpumon->cm_flags |= HWPERF_CM_ENABLED;
			ut->ut_cpumon->cm_flags &= ~HWPERF_CM_SUSPENDED;
			START_HWPERF_COUNTERS(ut);
		}
		return 0;
	}
	case HWPERF_RELSYSCNTRS:
	{
		if (!cap_able(CAP_DEVICE_MGT)) {
			rvp->r_val1 = -1;
			return EPERM;
		}
		rvp->r_val1 = hwperf_disable_sys_counters();
		return rvp->r_val1;
	}
	case HWPERF_GET_CPUCNTRS:
	{
		/* Read a particular cpu's counters */

		hwperf_cntr_t 	cntrs;
		int		rval, err;

		if ((err = hwperf_get_cpu_counters(uap->arg2, &cntrs,
						   &rval)) == 0) {
			rvp->r_val1 = rval;
			if (copyout((void *)&cntrs, (void *)uap->arg3,
					    sizeof(hwperf_cntr_t))) {
				rvp->r_val1 = -1;
				return EFAULT;
			}
		}
		return err;
	}
	case HWPERF_GET_SYSCNTRS:
	{

		/* Read the system's counters */

		hwperf_cntr_t 	cntrs;
		int		rval;

		if (error = hwperf_get_sys_counters(&cntrs, &rval))
			return error;

		rvp->r_val1 = rval;
		if (copyout((void *)&cntrs, (void *)uap->arg2,
					sizeof(hwperf_cntr_t))) {
			rvp->r_val1 = -1;
			return EFAULT;
		}
		return 0;
	}
	case HWPERF_GET_SYSEVCTRL:
	{
		/* 
		 * Get all the information about which
		 * events are being traced and the execution
		 * level for each event.
		 */
		hwperf_eventctrl_t 	evctrl;
		int			rval;		

		if (error = hwperf_sys_control_info(&evctrl, NULL,
						    &rval))
		    return error;

		rvp->r_val1 = rval;
		if (copyout((void *)&evctrl, (void *)uap->arg2,
				sizeof(hwperf_eventctrl_t))) {
			rvp->r_val1 = -1;
			return EFAULT;
		}
		return 0;
	}
	case HWPERF_SET_SYSEVCTRL:
	{
		/* 
		 * After acquiring the counters, change
		 * all/some of the events being tracked and/or the
		 * mode the event is being counted in.
		 */
		hwperf_profevctrarg_t 	evctr_args;
		int			rval;		
		
		if (!cap_able(CAP_DEVICE_MGT)) {
			rvp->r_val1 = -1;
			return EPERM;
		}
		if (copyin((void *)uap->arg2, 
				(void *)&evctr_args,
				sizeof(hwperf_profevctrarg_t))) {
			rvp->r_val1 = -1;
			return EFAULT;
		}

		error = hwperf_change_sys_control(&evctr_args,
						  HWPERF_SYS,
						  &rval);

		rvp->r_val1 = rval;
		return error;
	}

#if defined (SN0)
	case HWPERF_ERRCNT_ENABLE:
	{
		extern int hub_error_count_enable(__psunsigned_t);
		
		if (hub_error_count_enable(uap->arg2))
		    return EINVAL;

		return 0;
	}
	case HWPERF_ERRCNT_DISABLE:
	{
		extern int hub_error_count_disable(__psunsigned_t);
		
		if (hub_error_count_disable(uap->arg2))
		    return EINVAL;

		return 0;
	}
	case HWPERF_ERRCNT_GET:
	{
		void *err_cnt;
		int size;

		void *hub_error_count_get(__psunsigned_t, int *);
		
		err_cnt = hub_error_count_get(uap->arg2, &size);
		if (err_cnt == NULL) return EINVAL;

		if (copyout(err_cnt, (void *)uap->arg3, size)) {
			rvp->r_val1 = -1;
			return EFAULT;
		}
		
		return 0;
	}

	case MDPERF_ENABLE:
	case MDPERF_DISABLE:
	case MDPERF_GET_CTRL:
	case MDPERF_GET_COUNT:
	case MDPERF_GET_NODE_COUNT:
	{
		printf("This interface is now obsolete. See man page mdperf(2).\n");
		return EINVAL;
	}
	case MDPERF_NODE_ENABLE:
	{
		int rval;
		md_perf_control_t ctrl;

		if (copyin((void*)uap->arg3,(void*)&ctrl,sizeof(md_perf_control_t))) {
			return EFAULT;
		}
		
		error = md_perf_node_enable(&ctrl,(cnodeid_t)uap->arg2,&rval);
		rvp->r_val1 = rval;
		return error;
	}
	case MDPERF_NODE_DISABLE:
	{
		return md_perf_node_disable(uap->arg2);
	}
	case MDPERF_NODE_GET_CTRL:
	{
		md_perf_control_t ctrl;
		int rval;

		if (error = md_perf_get_node_ctrl(uap->arg2,&ctrl, &rval))
		    return error;

		if (copyout(&ctrl,(void*)uap->arg3,sizeof(md_perf_control_t)))
		    return EFAULT;

		rvp->r_val1 = rval;

		return 0;
	}
	case MDPERF_NODE_GET_COUNT:
	{
		md_perf_values_t cnt;
		int rval;

		if (error = md_perf_get_node_count(uap->arg2, &cnt, &rval))
		    return error;

		if (copyout((void *)&cnt, (void *)uap->arg3, 
			    sizeof(md_perf_values_t)))
		    return EFAULT;

		rvp->r_val1 = rval;

		return 0;
	}

	case IOPERF_ENABLE:
	{	
		int rval;
		io_perf_control_t ctrl;

		if (copyin((void*)uap->arg3,(void*)&ctrl,sizeof(io_perf_control_t))) {
			return EFAULT;
		}
		
		error = io_perf_node_enable(&ctrl,(cnodeid_t)uap->arg2, &rval);
		rvp->r_val1 = rval;
		return error;
	}
	case IOPERF_DISABLE:
	{
		return io_perf_node_disable((cnodeid_t)uap->arg2);
	}
	case IOPERF_GET_COUNT:
	{

		io_perf_values_t cnt;
		int rval;
		
		if (error = io_perf_get_node_count(uap->arg2, &cnt, &rval))
			return error;

		if (copyout((void *)&cnt, (void *)uap->arg3, sizeof(io_perf_values_t)))
			return EFAULT;

		rvp->r_val1 = rval;

		return 0;
	}

	case IOPERF_GET_CTRL:
	{
		io_perf_control_t ctrl;
		int rval;

		if (error = io_perf_get_node_ctrl(uap->arg2,&ctrl, &rval))
		    return error;

		if (copyout(&ctrl,(void*)uap->arg3,sizeof(io_perf_control_t)))
		    return EFAULT;

		rvp->r_val1 = rval;

		return 0;
	}

#endif /* SN0 */			
	default:
		return EINVAL;
	}
}
#endif /* R10000 */


static int
syssgi_ash(struct syssgia *uap, rval_t *rvp)
{
	switch (uap->cmd) {

        case SGI_SETASH:
	{
		ash_t oldash, newash;

		/*
		 * NOTE ABOUT ash_t & prid_t IN SYSSGI FUNCTIONS
		 *
		 * ash_t & prid_t are both 64-bit values, even on 32-bit
		 * systems. Because the arguments to syssgi are only the
		 * size of a long, ash_t's & prid_t's won't fit into
		 * a normal syssgi argument on 32-bit systems. Therefore,
		 * those values are always passed in with a pointer.
		 */

		/* Make sure user is eligible to change ASH */
		/* XXX Need better capability here?? */
		if (!cap_able(CAP_SETUID))
		    return EPERM;

		/* Get old ASH from user */
		if (uap->arg1 == -1)
		    oldash = -1LL;
		else if (copyin((caddr_t)uap->arg1, &oldash, sizeof(ash_t)))
		    return EFAULT;

		/* Get new ASH from user */
		if (copyin((caddr_t) uap->arg2, &newash, sizeof(ash_t)))
		    return EFAULT;

		return arsess_setash(oldash, newash);
	}

	case SGI_SETPRID:
	{
		ash_t  ash;
		prid_t prid;

		/* Make sure user is eligible to change project ID */
		/* XXX Need better capability here?? */
		if (!cap_able(CAP_SETUID))
		    return EPERM;

		/* Get ASH from user */
		if (uap->arg1 == -1)
		    ash = -1LL;
		else if (copyin((caddr_t)uap->arg1, &ash, sizeof(ash_t)))
		    return EFAULT;

		/* Get new project ID */
		if (copyin((caddr_t) uap->arg2, &prid, sizeof(prid_t)))
		    return EFAULT;

		return arsess_setprid(ash, prid);
	}

	case SGI_SETSPINFO:
	{
		ash_t *ashptr = NULL;

		if (uap->arg1 != -1) {
			ashptr = (ash_t *) uap->arg1;
		}

		return arsess_op(ARSOP_SETSPI,
				 ashptr,
				 (void *) uap->arg2,
				 sizeof(acct_spi_t));
	}

	case SGI_GETASH:
	{
		ash_t ash;
		pid_t pid = uap->arg1;
		vproc_t *vpr;
		vpagg_t *vpag;
		int rele = 0;

		if (pid >= MAXPID)
		    return EINVAL;

		/* If the 1st arg is < 0, get the handle of the vpag  */
		/* for the current process, otherwise treat it as a PID */

		if (pid <= 0 || pid == current_pid())
			vpr = curvprocp;
		else {
			if (pid >= MAXPID)
				return EINVAL;
			if ((vpr = VPROC_LOOKUP(pid)) == NULL)
				return ESRCH;
			rele = 1;
		}

		VPROC_GETVPAGG(vpr, &vpag);
		ash = vpag ? VPAG_GETPAGGID(vpag) : ASH_NONE;

		if (rele)
			VPROC_RELE(vpr);

		if (copyout(&ash, (void *) uap->arg2, sizeof(ash_t)))
		    return EFAULT;

		break;
	}

	case SGI_GETPRID:
	{
		vpagg_t *vpag;
		prid_t	prid;
		int rele = 0;
		

		/* If 1st arg is -1, use the arsess of the current  */
		/* process, otherwise it is an array session handle */
		if (uap->arg1 == -1) {
			VPROC_GETVPAGG(curvprocp, &vpag);
		} else {
			ash_t ash;

			if (copyin((caddr_t) uap->arg1, &ash, sizeof(ash_t)))
			    return EFAULT;
			vpag = VPAG_LOOKUP(ash);
			rele = 1;
		}
		if (vpag == 0)
		    return ESRCH;

		/* getprid sends a pointer in which to store the    */
		/* prid since prid's are 64 bits long, and so can't */
		/* be returned in the usual way for 32-bit kernels. */
		prid = VPAG_GETPRID(vpag);
		if(rele)
			VPAG_RELE(vpag);
		if (copyout(&prid,(void *) uap->arg2, sizeof(prid_t)))
		    return EFAULT;

		break;
	}


	case SGI_GETDFLTPRID:
	{
		if (copyout(&dfltprid, (void *) uap->arg1, sizeof(prid_t)))
		    return EFAULT;

		break;
	}
		

	case SGI_GETSPINFO:
	{
		ash_t *ashptr = NULL;

		if (uap->arg1 != -1) {
			ashptr = (ash_t *) uap->arg1;
		}

		return arsess_op(ARSOP_GETSPI,
				 ashptr,
				 (void *) uap->arg2,
				 sizeof(acct_spi_t));
	}

	case SGI_NEWARRAYSESS:
	{
		pid_t pid = uap->arg1;
		int result;

		/* If the 1st arg is <= 0, move the current process, */
		/* otherwise treat the arg as the PID to be moved.   */
		if (pid <= 0)
			pid = current_pid();
		else if (pid >= MAXPID)
			return EINVAL;

		/* Only privileged users can move another PID */
		if (pid != current_pid() && !cap_able(CAP_SETUID))
			return EPERM;

		result = arsess_new(pid);

		if (result < 0)
			return -result;	/* Error occurred */

		break;
	}

	case SGI_GETGRPPID:
	{
		struct scanhandle grp;
		int rc;

		grp.id = uap->arg1;
		grp.useraddr = (pid_t *)uap->arg2;
		grp.usermax = uap->arg3;

		if (rc = procscan(findgrp, &grp))
			return (rc);
		rvp->r_val1 = grp.count;
		break;
	}

	case SGI_GETSESPID:
	{
		struct scanhandle ses;
		int rc;

		ses.id = uap->arg1;
		ses.useraddr = (pid_t *)uap->arg2;
		ses.usermax = uap->arg3;

		if (rc = procscan(findses, &ses))
			return (rc);
		rvp->r_val1 = ses.count;

		break;
	}

	case SGI_PIDSINASH:
	{
		struct findashinfo fai;

		if (copyin((caddr_t) uap->arg1, &fai.ash, sizeof(ash_t)))
		    return EFAULT;
		fai.useraddr = (pid_t *) uap->arg2;
		fai.usermax  = uap->arg3;

		procscan(findash, &fai);
		if (fai.errno != 0)
		    return fai.errno;
		rvp->r_val1 = fai.count;

		break;
	}
	case SGI_ENUMASHS:
	{
		ash_t *ashlist;
		int   numashs;

		/* Asking for a non-positive # of ASHs is silly */
		if (uap->arg2 < 1) {
			return EINVAL;
		}

		/* Get the official list of ASHs in local storage */
		ashlist = (ash_t *) kern_calloc(uap->arg2,
						sizeof(ash_t));
		numashs = arsess_enumashs(ashlist, uap->arg2);

		/* Try to copy the list into user storage */
		if (copyout(ashlist, (void *) uap->arg1,
			    uap->arg2 * sizeof(ash_t))) {
			kern_free(ashlist);
			return EFAULT;
		}

		/* Clean up and return the number of ASHs, or croak */
		/* with ENOMEM if we didn't have enough space       */
		kern_free(ashlist);
		if (numashs < 0) {
			return ENOMEM;
		}
		rvp->r_val1 = numashs;
		break;
	}

	case SGI_SETASMACHID:
	{
		int result;

		if (!cap_able(CAP_SYSINFO_MGT)) {
			return EPERM;
		}

		result = arsess_setmachid(uap->arg1);
		if (result != 0) {
			return result;
		}

		break;
	}

	case SGI_GETASMACHID:
		rvp->r_val1 = arsess_getmachid();
		break;

	case SGI_GETARSESS:
	{
		arsess_t arsess_info;
		ash_t    ash;
		vpagg_t	*vpag;

		if (!cap_able(CAP_SYSINFO_MGT)) {
			return EPERM;
		}

		if (copyin((caddr_t) uap->arg1, &ash, sizeof(ash_t))) {
			return EFAULT;
		}

		vpag = VPAG_LOOKUP(ash);
		if (vpag == NULL) {
			return ESRCH;
		}


		VPAG_EXTRACT_ARSESS_INFO(vpag, &arsess_info);
		VPAG_RELE(vpag);
		if (copyout(&arsess_info, (void *) uap->arg2,
			    sizeof(arsess_t))) {
			return EFAULT;
		}
		break;
	}

	case SGI_JOINARRAYSESS:
	{
		ash_t ash;
		int result;
		pid_t pid = uap->arg1;

		/* Make sure user is eligible to change ASH's */
		if (!cap_able(CAP_SETUID)) {
			return EPERM;
		}

		/* If the 1st arg is <= 0, move the current process, */
		/* otherwise treat the arg as the PID to be moved.   */
		if (pid <= 0)
			pid = current_pid();
		else if (pid >= MAXPID)
			return EINVAL;

		/* Fetch the ASH from the user */
		if (copyin((caddr_t) uap->arg2, &ash, sizeof(ash_t))) {
			return EFAULT;
		}

		/* Either join or create array session with ASH */
		result = arsess_join(pid, ash);

		if (result < 0)
		    return -result;	/* Error occurred; errno is negative */

		break;
	}
	}
	return 0;
}


/*ARGSUSED2*/
static int
syssgi_rusage(struct syssgia *uap, int abi)
{
	struct	rusage ru;

	switch (uap->arg1) {
	case RUSAGE_SELF: 
		VPROC_GETRUSAGE(curvprocp, VRUSAGE_SELF, &ru);
		break;

	case RUSAGE_CHILDREN:
		VPROC_GETRUSAGE(curvprocp, 0, &ru);
		break;

	default:
		return EINVAL;
	}
	if (XLATE_COPYOUT(&ru, (void *)uap->arg2, sizeof ru,
			  rusage_to_irix5, abi, 1))
		return EFAULT;

	return 0;
}


#if IP32
extern void flash_dump_globals(void);
static int
syssgi_ip32(struct syssgia *uap)
{
	    /*
	     * uap->arg1 is pointer to PROM image.
	     * uap->arg2 is the length of that image.
	     * uap->arg3 is the starting offset at which
	     *           the image will be written.
	     */
	    char buf[FLASH_PAGE_SIZE];
	    caddr_t tp;
	    volatile caddr_t fp = (caddr_t)(0xbfc00000 + (int)uap->arg3);
	    
	    if (!_CAP_ABLE(CAP_NVRAM_MGT))
		return EPERM;

	    /* check that we don't overrun the flash address space */
	    if (((uint)fp + (uint)uap->arg2) > 
		((uint)0xbfc00000 + (uint)FLASH_SIZE))
		return ENXIO;

	    /* make sure the starting alignment meets restrictions */
	    if ((int)uap->arg3 & 0xff)
		return ENXIO;

#if DEBUG
	flash_dump_globals();
#endif
	    tp = (caddr_t)uap->arg1;
#ifdef WRITE_FLASH_DEBUG
	    cmn_err(CE_NOTE, "prom image 0x%x offset 0x%x len 0x%x start 0x%x",
		    tp, uap->arg3, uap->arg2, fp);
#endif
	    /*
	     * make sure that the entire buffer is valid.
	     */
	    if (useracc((caddr_t)uap->arg1, uap->arg2, B_WRITE, 0))
		return EFAULT;

	    /* force write of nvram environment at next shutdown */
	    flash_set_nvram_changed();

	    while (tp < (caddr_t)((uint)uap->arg1 + (uint)uap->arg2)) {
		/*
		 * return EIO here because EIO means "you're 
		 * f*cked, buddy".  This allows differentiation
		 * between those cases where the PROM is left
		 * intact and where the image is partially 
		 * written.
		 */
		if (copyin(tp, buf, FLASH_PAGE_SIZE)) {
		    unuseracc((caddr_t)uap->arg1, uap->arg2, B_READ);
		    return EIO;
		}

#ifdef WRITE_FLASH_DEBUG
		cmn_err(CE_NOTE, "writing 0x%x bytes at 0x%x", 
			FLASH_PAGE_SIZE, fp);
#endif
		if (flash_write_sector(fp, buf)) {
		    unuseracc((caddr_t)uap->arg1, uap->arg2, B_READ);
		    return EIO;
		}
		tp += FLASH_PAGE_SIZE; fp += FLASH_PAGE_SIZE;
	    }

	    /*
	     * At this point, global information being held in variables in IP32flash.c
	     * (such as the size and location of the environment segment in the prom)
	     * have to be updated, since the PROM layout is (potentially) different.
	     * We recalculate these now, specifically before flash_write_env() 
	     * attempts to use them to write the saved environment back to NVRAM.
	     */
	    flash_sync_globals();
#if DEBUG
	    flash_dump_globals();
#endif

	    unuseracc((caddr_t)uap->arg1, uap->arg2, B_READ);

	    /*
	     * write back the environment, possibly to a new location if the
	     * "env" segment has moved.
	     */
	    flash_write_env();

	return 0;
}
#endif /* IP32 */

static int
syssgi_tstamp(struct syssgia *uap, rval_t *rvp)
{
	int error = 0;
#if (_MIPS_SIM == _ABI64)
	int abi = get_current_abi();
#endif

	switch (uap->cmd) {
	case SGI_RT_TSTAMP_CREATE:
		if (cap_able(CAP_SYSINFO_MGT)) {
			__psint_t addr;
			error = tstamp_user_create((int)uap->arg1,
			       (int)uap->arg2,
			       (paddr_t*)&addr);
			rvp->r_val1 = addr;
		} else
			error = EPERM;
		break;
        case SGI_RT_TSTAMP_DELETE:
		error = cap_able(CAP_SYSINFO_MGT) ?
			tstamp_user_delete((int)uap->arg1) : EPERM;
		break;
        case SGI_RT_TSTAMP_START:
        case SGI_RT_TSTAMP_STOP:
		return(ENOTSUP);
        case SGI_RT_TSTAMP_ADDR:
		if (cap_able(CAP_SYSINFO_MGT)) {
			paddr_t rval1;
			error = tstamp_user_addr((int)uap->arg1, &rval1);
			rvp->r_val1 = rval1;
		} else
			error = EPERM;
		break;
        case SGI_RT_TSTAMP_MASK:
		if (cap_able(CAP_SYSINFO_MGT)) {
			uint64_t omask;
#if (_MIPS_SIM == _ABI64)
			if (ABI_HAS_64BIT_REGS(abi)) {
				error = tstamp_user_mask((int)uap->arg1,
					    (uint64_t) uap->arg2, &omask);
				if (!error && uap->arg3)
					error = copyout(&omask,
					   (caddr_t) uap->arg3, sizeof (omask));
			} else
#endif
			{
				uint64_t mask;
				/*
				 * o32 and n32 kernels will put the 64-bit
				 * event mask into arg2 and arg3 because those
				 * kernels use 32-bit sysarg_t's.  Note that
				 * the SGI_RT_TSTAMP_MASK and <cpu> arguments
				 * to syssgi() leave the mask already 8-byte
				 * aligned.  If they hadn't the 32-bit user ABI
				 * would have introduced a 32-bit pad to 8-byte
				 * align the mask argument.
				 */
				mask = ((uint64_t)(uap->arg2 & 0xffffffff)<<32)
				      | (uint64_t)(uap->arg3 & 0xffffffff);
				error = tstamp_user_mask((int)uap->arg1,
					    mask, &omask);
				if (!error && uap->arg4)
					error = copyout(&omask,
					   (caddr_t) uap->arg4, sizeof (omask));
			}
		} else
			error = EPERM;
		break;
        case SGI_RT_TSTAMP_EOB_MODE:
		if (cap_able(CAP_SYSINFO_MGT)) {
			uint omode;
			error = tstamp_user_eob_mode((int)uap->arg1,
				    (uint) uap->arg2, &omode);
			if (!error && uap->arg3)
				error = copyout(&omode,
					   (caddr_t) uap->arg3, sizeof (omode));
		} else
			error = EPERM;
		break;
	/*
	 * NB: Cannot restrict SGI_RT_TSTAMP_WAIT access since
	 * rtmon_log_user_tstamp uses it and clients are
	 * typically not privileged.  Besides it's not really
	 * an issue if someone wants to make the call.  We
	 * also don't restrict SGI_RT_TSTAMP_UPDATE because
	 * clients that monitor the event buffer use this
	 * heavily and the cost to do a cap_able check is
	 * too high (sigh).  Letting a random user access
	 * this call cannot leak info though it can potentially
	 * screwup event collection logic (but not in a way
	 * that should threaten a system).
	 */
        case SGI_RT_TSTAMP_WAIT:
		error = tstamp_user_wait((int)uap->arg1, (int)uap->arg2);
		break;
	case SGI_RT_TSTAMP_UPDATE:
		error = tstamp_user_update((int)uap->arg1, (int)uap->arg2);
		break;
	}
	return (error);
}

/*
 * findgrp:
 *	iteratively called by procscan to search for processes belonging
 *	to a given group. Used by SGI_GETGRPPID.
 */
static int
findgrp(proc_t *p, void *arg, int mode)
{
	struct scanhandle *gp = (struct scanhandle *) arg;

	switch (mode) {

	case 0:
		gp->count = 0;
		break;

	case 1:
		/*
		 * We don't really want to be dereferencing p_vpgrp
		 * without any kind of lock, but we can't completely
		 * trust p->p_pgid, because if the process exits, it
		 * leaves p_pgid set, to be perused by parent, until
		 * the corpse is disposed.  So, we use p_proup to
		 * indicate that the process group is valid, and use
		 * p_pgid as the cached process group id.
		 */
		if (p->p_vpgrp  &&  p->p_pgid == gp->id) {
			if (gp->useraddr == NULL) {
				gp->count++;
				break;
			}
			if (gp->count == gp->usermax)
				return (ENOMEM);

			if (copyout(&p->p_pid,
				    (caddr_t)(gp->useraddr + gp->count),
				    sizeof(pid_t)))
				return (EFAULT);

			gp->count++;
		}
		break;

	case 2:
	default:
		break;
	}

	return 0;
}

/*
 * findses:
 *	iteratively called by procscan to search for processes belonging
 *	to a given session. Used by SGI_GETSESPID.
 */
static int
findses(proc_t *p, void *arg, int mode)
{
	struct scanhandle *sp = (struct scanhandle *) arg;
	int s;

	switch (mode) {

	case 0:
		sp->count = 0;
		break;

	case 1:
		s = p_lock(p);
		if (p->p_sid == sp->id) {
			p_unlock(p, s);
			if (sp->useraddr == NULL) {
				sp->count++;
				break;
			}
			if (sp->count == sp->usermax)
				return (ENOMEM);

			if (copyout(&p->p_pid,
				    (caddr_t)(sp->useraddr + sp->count),
				    sizeof(pid_t)))
				return (EFAULT);

			sp->count++;
			break;
		}
		p_unlock(p, s);
		break;

	case 2:
	default:
		break;
	}

	return 0;
}

/*
 * findash:
 *	iteratively called by procscan to search for processes belonging
 *	to a given ash. Used by SGI_PIDSINASH.
 */
static int
findash(proc_t *p, void *arg, int mode)
{
	struct findashinfo *fai = (struct findashinfo *) arg;

	switch (mode) {

	case 0:
		fai->count = 0;
		fai->current = 0;
		fai->errno = 0;
		break;

	case 1:
		if (p->p_vpagg  &&  VPAG_GETPAGGID(p->p_vpagg) == fai->ash) {
			if (fai->useraddr == NULL) {
				fai->count++;
				break;
			}
			if (fai->current == fai->usermax) {
				fai->errno = ENOMEM;
				return -1;
			}

			if (copyout(&p->p_pid,
				    (caddr_t) (fai->useraddr + fai->current),
				    sizeof(pid_t)))
			{
				fai->errno = EFAULT;
				return -1;
			}

			++fai->count;
			++fai->current;
		}
		break;

	case 2:
	default:
		break;
	}

	return 0;
}

#if _MIPS_SIM == _ABI64
/* ARGSUSED */
int
rusage_to_irix5(
	void *from,
	int count,
	register xlate_info_t *info)
{
	register struct irix5_rusage *i5_rp;
	register struct rusage *rp = from;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	if (sizeof(struct irix5_rusage) <= info->inbufsize)
		info->copybuf = info->smallbuf;
	else
		info->copybuf = kern_malloc(sizeof(struct irix5_rusage));
	info->copysize = sizeof(struct irix5_rusage);

	i5_rp = info->copybuf;
	timeval_to_irix5(&rp->ru_utime, &i5_rp->ru_utime);
	timeval_to_irix5(&rp->ru_stime, &i5_rp->ru_stime);
	i5_rp->ru_maxrss = rp->ru_maxrss;
	i5_rp->ru_ixrss = rp->ru_ixrss;
	i5_rp->ru_idrss = rp->ru_idrss;
	i5_rp->ru_isrss = rp->ru_isrss;
	i5_rp->ru_minflt = rp->ru_minflt;
	i5_rp->ru_majflt = rp->ru_majflt;
	i5_rp->ru_nswap = rp->ru_nswap;
	i5_rp->ru_inblock = rp->ru_inblock;
	i5_rp->ru_oublock = rp->ru_oublock;
	i5_rp->ru_msgsnd = rp->ru_msgsnd;
	i5_rp->ru_msgrcv = rp->ru_msgrcv;
	i5_rp->ru_nsignals = rp->ru_nsignals;
	i5_rp->ru_nvcsw = rp->ru_nvcsw;
	i5_rp->ru_nivcsw = rp->ru_nivcsw;

	return 0;
}

/* ARGSUSED */
static int
invent_to_irix5(
	void *from,
	int count,
	register xlate_info_t *info)
{
	register irix5_inventory_t *i5_inv;
	register inventory_t *inv = from;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	if (sizeof(irix5_inventory_t) <= info->inbufsize)
		info->copybuf = info->smallbuf;
	else
		info->copybuf = kern_malloc(sizeof(irix5_inventory_t));
	info->copysize = sizeof(irix5_inventory_t);

	i5_inv = info->copybuf;

	i5_inv->inv_next = 0;
	i5_inv->inv_class = inv->inv_class;
	i5_inv->inv_type = inv->inv_type;
	i5_inv->inv_controller = inv->inv_controller;
	i5_inv->inv_unit = inv->inv_unit;
	i5_inv->inv_state = inv->inv_state;

	return 0;
}

/* ARGSUSED */
static int
irix5_to_iospace(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	register iospace_t *ios;
	register irix5_iospace_t *i5_ios;

	ASSERT(info->smallbuf != NULL);
	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);

	if (mode == SETUP_BUFFER) {
		ASSERT(info->copybuf == NULL);
		ASSERT(info->copysize == 0);
		if (sizeof(irix5_iospace_t) <= info->inbufsize)
			info->copybuf = info->smallbuf;
		else
			info->copybuf = kern_malloc(sizeof(irix5_iospace_t));
		info->copysize = sizeof(irix5_iospace_t);
		return 0;
	}

	ASSERT(info->copysize == sizeof(irix5_iospace_t));
	ASSERT(info->copybuf != NULL);

	ios = to;
	i5_ios = info->copybuf;

	ios->ios_type = i5_ios->ios_type;
	ios->ios_iopaddr = i5_ios->ios_iopaddr;
	ios->ios_size = i5_ios->ios_size;
	ios->ios_vaddr = 0;			/* Not an input parameter. */

	return 0;
}

#endif	/* _ABI64 */

/* ARGSUSED */
int
phost_systune(
	bhv_desc_t	*bdp,
	int		group_index,
	int		tune_index,
	uint64_t	value)
{

	struct tunetable *tp = &tunetable[group_index];
	struct tunename *tn = &tunename[tune_index];
	typedef int (*tsan_t)(int *, int);
	typedef int (*tsan64_t)(uint64_t *, uint64_t);
	extern int tuneentries;
	extern int tunename_cnt;

	/*
	 * If there is a sanity function - it
	 * is responsible for updating the value
	 * otherwise, we set it here.  Since the value is always
	 * passed as 64-bit we may need to convert it.
	 */

	ASSERT((uint)group_index < tuneentries);
	ASSERT((uint)tune_index < tunename_cnt);

	if (tn->t_size == sizeof(uint64_t)) {
		if (tp->t_sanity) {
			if ((*(tsan64_t)(tp->t_sanity))((uint64_t *)tn->t_addr,
					 value))
				return EINVAL;
		} else {
			*(uint64_t *)tn->t_addr = value;
		}
	}
	else {
		if (tp->t_sanity) {
			if ((*(tsan_t)(tp->t_sanity))(tn->t_addr, (int)value))
				return EINVAL;
		}
		else {
			*tn->t_addr = (int)value;
		}
	}
	return(0);
}

/*
 *
 */

static
int
kid_is_uthread(
	proc_t	*p,
	void	*arg,
	int	mode)
{
	struct kiu_arg	*uarg = (struct kiu_arg *)arg;
	uthread_t	*ut;

	if (mode == 1) {
		uscan_access(&p->p_proxy);
		for (ut = prxy_to_thread(&p->p_proxy); ut; ut = ut->ut_next) {
			if (UT_TO_KT(ut)->k_id == uarg->kid) {
				uarg->pid = p->p_pid;
				strncpy(uarg->name, p->p_comm, PSCOMSIZ);
				uscan_unlock(&p->p_proxy);
				return(1);
			}
		}
		uscan_unlock(&p->p_proxy);
	}
	return(0);
}

