/*
 * priface.h
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

#ident "$Revision: 1.50 $"

#ifndef _FS_PROCFS_PRIFACE_H
#define _FS_PROCFS_PRIFACE_H

#include <sys/types.h>
#include <ksys/as.h>
#include <sys/ksignal.h>

#if _KERNEL				/* Whole file */

	/* Note that the register format for n32 and n64 are the same */

#define priface_gregset_copyin(f, t)					\
		(ABI_IS_IRIX5_64(target_abi) ||				\
		 ABI_IS_IRIX5_N32(target_abi) ?				\
		copyin(f, t, sizeof(irix5_64_gregset_t)) :		\
		copyin(f, t, sizeof(irix5_gregset_t)))

#define priface_gregset_copyout(f, t)					\
		(ABI_IS_IRIX5_64(target_abi) ||				\
		 ABI_IS_IRIX5_N32(target_abi) ?				\
		copyout(f, t, sizeof(irix5_64_gregset_t)) :		\
		copyout(f, t, sizeof(irix5_gregset_t)))

#define priface_fpregset_copyin(f, t)					\
		(ABI_IS_IRIX5_64(target_abi) ||				\
		 ABI_IS_IRIX5_N32(target_abi) ?				\
		copyin(f, t, sizeof(irix5_64_fpregset_t)) :		\
		copyin(f, t, sizeof(irix5_fpregset_t)))

#define priface_fpregset_copyout(f, t)					\
		(ABI_IS_IRIX5_64(target_abi) ||				\
		 ABI_IS_IRIX5_N32(target_abi) ?				\
		copyout(f, t, sizeof(irix5_64_fpregset_t)) :		\
		copyout(f, t, sizeof(irix5_fpregset_t)))


/* OLSON: need to check this one out.  I'm pretty sure we want the
 * o32 way here, but we may need a hybrid of the two. Will definitely
 * have to deal with getting o32 registers on an n32 kernel */
 /* Danger, will robinson! */

#if _MIPS_SIM == _ABIO32

	/* copyin/copyout defines */
#define priface_prpsinfo_copyout(f, t)	copyout(f, t, sizeof(prpsinfo_t))
#define priface_sigaction_copyout(f, t)	copyout(f, t, sizeof(sigaction_t))
#define priface_prusage_copyout(f, t)	copyout(f, t, sizeof(prusage_t))

	/* Routines to fill out structures */
#define	priface_prgetregs(ut, buf)					\
		(ABI_IS_IRIX5_N32(target_abi) ?				\
		 prgetregs_irix5_64(ut, (irix5_64_greg_t *) buf) :	\
		 prgetregs(ut, (greg_t *) buf))

#define	priface_prsetregs(ut, buf)					\
		(ABI_IS_IRIX5_N32(target_abi) ?				\
		 prsetregs_irix5_64(ut, (irix5_64_greg_t *) buf) :	\
		 prsetregs(ut, (greg_t *) buf))

#define	priface_prgetfpregs(ut, buf)					 \
		(ABI_IS_IRIX5_N32(target_abi) ?				 \
		 prgetfpregs_irix5_64(ut, (irix5_64_fpregset_t *) buf) : \
		 prgetfpregs(ut, (fpregset_t *) buf))

#define	priface_prsetfpregs(ut, buf)					 \
		(ABI_IS_IRIX5_N32(target_abi) ?				 \
		 prsetfpregs_irix5_64(ut, (irix5_64_fpregset_t *) buf) : \
		 prsetfpregs(ut, (fpregset_t *) buf))


#define	priface_prgetpsinfo(ut, p, for_proc, buf)	\
				prgetpsinfo(ut, p, for_proc, (prpsinfo_t *)buf)
#define	priface_prgetaction(p, sig, act)	\
				prgetaction(p, sig, (sigaction_t *)act)
#define	priface_prgetpgd_sgi(v, va, buf)	\
				prgetpgd_sgi(v, va, (prpgd_sgi_t *)buf)
#define	priface_prgetusage(p, buf)	prgetusage(p, (prusage_t *)buf)
#define	priface_prgetptimer(p, buf)	prgetptimer(p, (timespec_t *)buf)

	/* Oddballs and misc. */
#define priface_sizeof_sigaction_t()	sizeof(sigaction_t)
#define priface_sizeof_prwatch_t()	sizeof(prwatch_t)
#define priface_sizeof_timespec_t()	sizeof(timespec_t)
#define priface_watchcopy		watchcopy

#endif

#if _MIPS_SIM == _ABI64 ||  _MIPS_SIM == _ABIN32
/*OLSON: n32 *really* the same as 64?  probably */

	/* copyin/copyout defines */

#define priface_prpsinfo_copyout(f, t)					\
		(ABI_IS_IRIX5_64(target_abi) ?				\
		copyout(f, t, sizeof(prpsinfo_t)) :			\
		copyout(f, t, sizeof(irix5_prpsinfo_t)))

#define priface_sigaction_copyout(f, t)					\
		(ABI_IS_IRIX5_64(target_abi) ?				\
		copyout(f, t, sizeof(sigaction_t)) :			\
		copyout(f, t, sizeof(irix5_sigaction_t)))

#define priface_prusage_copyout(f, t)					\
		(ABI_IS_IRIX5_64(target_abi) ?				\
		copyout(f, t, sizeof(prusage_t)) :			\
		copyout(f, t, sizeof(irix5_prusage_t)))

	/* Routines to fill out structures */

	/* Note that the register format for n32 and n64 are the same */
#define	priface_prgetregs(ut, buf)					\
		(ABI_IS_IRIX5_64(target_abi) ||				\
		 ABI_IS_IRIX5_N32(target_abi) ?				\
		prgetregs(ut, (greg_t *)buf) :				\
		irix5_prgetregs(ut, (irix5_greg_t *)buf))

#define	priface_prsetregs(ut, buf)					\
		(ABI_IS_IRIX5_64(target_abi) ||				\
		 ABI_IS_IRIX5_N32(target_abi) ?				\
		prsetregs(ut, (greg_t *)buf) :				\
		irix5_prsetregs(ut, (irix5_greg_t *)buf))

#define	priface_prgetfpregs(ut, buf)					\
		(ABI_IS_IRIX5_64(target_abi) ||				\
		 ABI_IS_IRIX5_N32(target_abi) ?				\
		prgetfpregs(ut, (fpregset_t *)buf) :			\
		irix5_prgetfpregs(ut, (irix5_fpregset_t *)buf))

#define	priface_prsetfpregs(ut, buf)					\
		(ABI_IS_IRIX5_64(target_abi) ||				\
		 ABI_IS_IRIX5_N32(target_abi) ?				\
		prsetfpregs(ut, (fpregset_t *)buf) :			\
		irix5_prsetfpregs(ut, (irix5_fpregset_t *)buf))

#define	priface_prgetpsinfo(ut, p, for_proc, buf)			\
		(ABI_IS_IRIX5_64(target_abi) ?				\
		prgetpsinfo(ut, p, for_proc, (prpsinfo_t *)buf) :	\
		irix5_prgetpsinfo(ut, p, for_proc, (irix5_prpsinfo_t *)buf))

#define	priface_prgetaction(p, sig, act)				\
		(ABI_IS_IRIX5_64(target_abi) ?				\
		prgetaction(p, sig, (sigaction_t *)act) :		\
		irix5_prgetaction(p, sig, (irix5_sigaction_t *)act))

#define	priface_prgetpgd_sgi(v, va, buf)					\
		(ABI_IS_IRIX5_64(target_abi) ?				\
		prgetpgd_sgi(v, va, (prpgd_sgi_t *)buf) :		\
		irix5_prgetpgd_sgi(v, va, (irix5_prpgd_sgi_t *)buf))

#define	priface_prgetusage(p, buf)					\
		(ABI_IS_IRIX5_64(target_abi) ?				\
		prgetusage(p, (prusage_t *)buf) :			\
		irix5_prgetusage(p, (irix5_prusage_t *)buf))

#define	priface_prgetptimer(p, buf)					\
		(ABI_IS_IRIX5_64(target_abi) ?				\
		prgetptimer(p, (timespec_t *)buf) :			\
		irix5_prgetptimer(p, (irix5_timespec_t *)buf))

	/* Oddballs and misc. */
#define priface_sizeof_sigaction_t()					\
		(ABI_IS_IRIX5_64(target_abi) ?				\
		sizeof(sigaction_t) :					\
		sizeof(irix5_sigaction_t))

#define priface_sizeof_prwatch_t()					\
		(ABI_IS_IRIX5_64(target_abi) ?				\
		sizeof(prwatch_t) :					\
		sizeof(irix5_prwatch_t))

#define priface_sizeof_timespec_t()					\
		(ABI_IS_IRIX5_64(target_abi) ?				\
		sizeof(timespec_t) :					\
		sizeof(irix5_timespec_t))

#define priface_watchcopy						\
		(ABI_IS_IRIX5_64(target_abi) ?				\
		watchcopy :						\
		irix5_watchcopy)

#endif /* _ABI64 || _ABIN32 */

#ifdef R10000
#define priface_hwpcntrs_accessible(p)					\
		(get_current_cred()->cr_uid == (p)->p_cred->cr_uid ||	\
		 	cap_able(CAP_DEVICE_MGT))
#endif

/* irix5 structure definitions. */
typedef struct irix5_prstatus {
	app32_ulong_t	pr_flags;	/* Process flags */
	short		pr_why;		/* Reason for process stop */
	short		pr_what;	/* More detailed reason */
	short		pr_cursig;	/* Current signal */
	short		pr_nthreads;	/* Number of kernel threads */
	sigset_t	pr_sigpend;	/* Set of pending signals */
	sigset_t	pr_sighold;	/* Set of held signals */
	irix5_siginfo_t	pr_info;	/* info assoc. with signal or fault */
	struct irix5_sigaltstack pr_altstack; /* Alternate signal stack info */
	irix5_sigaction_t pr_action;	/* Signal action for current signal */
	app32_long_t	pr_syscall;	/* syscall number (if in syscall) */
	app32_long_t	pr_nsysarg;	/* number of arguments to syscall */
	app32_long_t	pr_errno;	/* error number from system call */
	app32_long_t	pr_rval1;	/* syscall return value 1 */
	app32_long_t	pr_rval2;	/* syscall return value 2 */
	app32_long_t	pr_sysarg[PRSYSARGS];	/* syscall arguments */
	irix5_pid_t	pr_pid;		/* Process id */
	irix5_pid_t	pr_ppid;	/* Parent process id */
	irix5_pid_t	pr_pgrp;	/* Process group id */
	irix5_pid_t	pr_sid;		/* Session id */
	irix5_timespec_t pr_utime;	/* Process user cpu time */
	irix5_timespec_t pr_stime;	/* Process system cpu time */
	irix5_timespec_t pr_cutime;	/* Sum of children's user times */
	irix5_timespec_t pr_cstime;	/* Sum of children's system times */
	char		pr_clname[8];	/* Scheduling class name */
	union {
		app32_long_t pr_filler[20]; /* Filler area */
		struct {
			sigset_t sigpnd;/* Set of signals pending on thread */
			id_t	 who;	/* Which kernel thread */
		} pr_st;
	} pr_un;
	inst_t		pr_instr;	/* Current instruction */
	irix5_gregset_t	pr_reg;		/* General registers */
} irix5_prstatus_t;

#define	pr_thsigpend	pr_un.pr_st.sigpnd
#define	pr_who		pr_un.pr_st.who

typedef struct irix5_prpsinfo {
	char	pr_state;	/* numeric process state */
	char	pr_sname;	/* printable character representing pr_state */
	char	pr_zomb;	/* !=0: process exited but not waited for */
	char	pr_nice;	/* process priority */
	app32_ulong_t	pr_flag;	/* process flags */
	irix5_uid_t	pr_uid;		/* real user id */
	irix5_gid_t	pr_gid;		/* real group id */
	irix5_pid_t	pr_pid;		/* process id */
	irix5_pid_t	pr_ppid;	/* parent process id */
	irix5_pid_t	pr_pgrp;	/* pid of process group leader */
	irix5_pid_t	pr_sid;		/* session id */
	app32_ptr_t	pr_addr;	/* physical address of process */
	app32_long_t	pr_size;	/* process image size in pages */
	app32_long_t	pr_rssize;	/* resident set size in pages */
	app32_ptr_t	pr_wchan;	/* wait addr for sleeping process */
	irix5_timespec_t pr_start;	/* process start time */
	irix5_timespec_t pr_time;	/* user+sys time for this process */
	app32_long_t	pr_pri;		/* priority */
	app32_long_t	pr_oldpri;	/* pre-svr4 priority */
	char	pr_cpu;			/* pre-svr4 cpu usage */
	dev_t	pr_ttydev;		/* controlling tty */
	char	pr_clname[8];		/* scheduling class name */
	char	pr_fname[PRCOMSIZ];	/* basename of exec()'d pathname */
	char	pr_psargs[PRARGSZ];	/* initial chars of arg list */
	uint	pr_spare1;		/* spare */
	cpuid_t	pr_sonproc;		/* processor running on */
	irix5_timespec_t pr_ctime;	/* user+sys time for all children */
	irix5_uid_t	pr_shareuid;	/* uid of ShareII lnode */
	app32_long_t	pr_spid;	/* sproc pid */
	irix5_timespec_t pr_qtime;	/* cumulative time process */
	char	pr_wname[16];		/* name of lock represented by pr_wchan */
	app32_long_t	pr_thds;	/* threads */
	app32_long_t	pr_fill[8];	/* spares */
} irix5_prpsinfo_t;

typedef struct irix5_prmap {
	app32_ptr_t	pr_vaddr;	/* Virtual base address */
	app32_ulong_t	pr_size;	/* Size of mapping in bytes */
	irix5_off_t	pr_off;		/* Offset into mapped object, if any */
	app32_ulong_t	pr_mflags;	/* Protection and attribute flags */
	app32_long_t	pr_filler[4];	/* for future expansion */
} irix5_prmap_t ;

typedef struct irix5_prmap_sgi {
	app32_ptr_t	pr_vaddr;	/* Virtual base address */
	app32_ulong_t	pr_size;	/* Size of mapping in bytes */
	irix5_off_t	pr_off;		/* Offset into mapped object, if any */
	app32_ulong_t	pr_mflags;	/* Protection and attribute flags */
	app32_ulong_t	pr_vsize;	/* # of valid pages in region */
	app32_ulong_t	pr_psize;	/* # of private pages in this region */
	app32_ulong_t	pr_wsize;	/* usage counts for valid pages  */
	app32_ulong_t	pr_rsize;       /* # of referenced pages */
	app32_ulong_t	pr_msize;       /* # of modified pages */
	app32_ulong_t	pr_dev;		/* Device # of region iff mapped */
	app32_ulong_t	pr_ino;		/* Inode # of region iff mapped */
	ushort_t	pr_lockcnt;	/* lock count */
	char            pr_filler[5*sizeof(app32_long_t)-sizeof(ushort_t)];
					/* for future expansion */
} irix5_prmap_sgi_t ;

typedef struct irix5_prmap_sgi_arg {
	app32_ptr_t	pr_vaddr;       /* Virtual ptr. to base of map buffer */
	app32_ulong_t	pr_size;        /* Size of user's map buffer in bytes */
} irix5_prmap_sgi_arg_t;

typedef struct irix5_prpgd_sgi {
	app32_ptr_t	pr_vaddr;	/* Virtual base address */
	app32_long_t	pr_pglen;	/* Number of pages in data array */
	pgd_t		pr_data[1];	/* Array of page flags */
} irix5_prpgd_sgi_t;

typedef struct irix5_prwatch {
	app32_ptr_t	pr_vaddr;
	app32_ulong_t	pr_size;
	app32_ulong_t	pr_wflags;
	app32_long_t	pr_filler;
} irix5_prwatch_t;

typedef struct irix5_prusage {
	irix5_timespec_t pu_tstamp;	/* time stamp */ 
	irix5_timespec_t pu_starttime;	/* time process was started */ 
	irix5_timespec_t pu_utime;	/* user CPU time */ 
	irix5_timespec_t pu_stime;	/* system CPU time */ 
	app32_ulong_t pu_minf;		/* minor (mapping) page faults */
	app32_ulong_t pu_majf;		/* major (disk) page faults */
	app32_ulong_t pu_utlb;		/* user TLB misses */
	app32_ulong_t pu_nswap;		/* swaps (process only) */
	app32_ulong_t pu_gbread;	/* gigabytes ... */ 
	app32_ulong_t pu_bread;		/* and bytes read */
	app32_ulong_t pu_gbwrit;	/* gigabytes ... */ 
	app32_ulong_t pu_bwrit;		/* and bytes written */
	app32_ulong_t pu_sigs;		/* signals received */
	app32_ulong_t pu_vctx;		/* voluntary context switches */
	app32_ulong_t pu_ictx;		/* involuntary context switches */
	app32_ulong_t pu_sysc;		/* system calls */ 
	app32_ulong_t pu_syscr;		/* read() system calls */ 
	app32_ulong_t pu_syscw;		/* write() system calls */ 
	app32_ulong_t pu_syscps;	/* poll() or select() system calls */
	app32_ulong_t pu_sysci;		/* ioctl() system calls */ 
	app32_ulong_t pu_graphfifo;	/* graphics pipeline stalls */
	app32_ulong_t pu_graph_req[8];	/* graphics resource requests */
	app32_ulong_t pu_graph_wait[8];	/* graphics resource waits */
	app32_ulong_t pu_size;		/* size of swappable image in pages */
	app32_ulong_t pu_rss;		/* resident set size */
	app32_ulong_t pu_inblock;	/* block input operations */
	app32_ulong_t pu_oublock;	/* block output operations */
	app32_ulong_t pu_vfault;	/* total number of vfaults */
	app32_ulong_t pu_ktlb;		/* kernel TLB misses */
} irix5_prusage_t; 

typedef struct irix5_prrun {
	app32_ulong_t	pr_flags;	/* Flags */
	sigset_t	pr_trace;	/* Set of signals to be traced */
	sigset_t	pr_sighold;	/* Set of signals to be held */
	fltset_t	pr_fault;	/* Set of faults to be traced */
	app32_ptr_t	pr_vaddr;	/* Virt. address at which to resume */
	app32_long_t	pr_filler[8];	/* Filler area for future expansion */
} irix5_prrun_t ;

typedef struct irix5_prthreadctl {
	tid_t		pt_tid;		/* Id of the designated thread */
        int             pt_cmd;         /* Command value for nested ioctl */
        int             pt_flags;       /* Flags governing use of pt tid */
        app32_ptr_t     pt_data;        /* Data pointer for nested command. */
} irix5_prtheadctl_t;


struct proc;
void		irix5_prgetaction(struct proc *, u_int,
			    	  struct irix5_sigaction *);
void		irix5_prgetregs(struct uthread_s *, irix5_greg_t *);
void		irix5_prsetregs(struct uthread_s *, irix5_greg_t *);
int		irix5_prgetfpregs(struct uthread_s *, irix5_fpregset_t *);
int		irix5_prsetfpregs(struct uthread_s *, irix5_fpregset_t *);
int		irix5_prgetpsinfo(struct uthread_s *, struct prnode *, int,
				  struct irix5_prpsinfo *);
int		irix5_watchcopy(caddr_t, ulong, ulong, caddr_t *);
void		irix5_prgetusage(struct proc *, irix5_prusage_t *);
void		irix5_prgetptimer(struct proc *, irix5_timespec_t *);
int		irix5_prgetpgd_sgi(caddr_t, vasid_t, irix5_prpgd_sgi_t *);

#if (_MIPS_SIM == _ABI64)
extern int irix5_to_prrun(enum xlate_mode, void *, int, xlate_info_t *);
extern int irix5_to_prwatch(enum xlate_mode, void *, int, xlate_info_t *);
extern int irix5_to_prthreadctl(enum xlate_mode, void *, int, xlate_info_t *);
#endif

#endif	/* _KERNEL */
#endif	/* _FS_PROCFS_PRIFACE_H */
