/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef _FS_PROCFS_PROCFS_H	/* wrapper symbol for kernel use */
#define _FS_PROCFS_PROCFS_H	/* subject to change without notice */

#ident	"$Revision: 1.101 $"

#include <sys/extacct.h>
#include <sys/fault.h>
#include <sys/siginfo.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/timespec.h>
#include <sys/ucontext.h>

/*
 * ioctl codes and system call interfaces for /proc.
 */
#define PIOC		('q'<<8)
#define PIOCSTATUS	(PIOC|1)	/* get process status */
#define PIOCSTOP	(PIOC|2)	/* post STOP request and... */
#define PIOCWSTOP	(PIOC|3)	/* wait for process to STOP */
#define PIOCRUN		(PIOC|4)	/* make process runnable */
#define PIOCGTRACE	(PIOC|5)	/* get traced signal set */
#define PIOCSTRACE	(PIOC|6)	/* set traced signal set */
#define PIOCSSIG	(PIOC|7)	/* set current signal */
#define PIOCKILL	(PIOC|8)	/* send signal */
#define PIOCUNKILL	(PIOC|9)	/* delete a signal */
#define PIOCGHOLD	(PIOC|10)	/* get held signal set */
#define PIOCSHOLD	(PIOC|11)	/* set held signal set */
#define PIOCMAXSIG	(PIOC|12)	/* get max signal number */
#define PIOCACTION	(PIOC|13)	/* get signal action structs */
#define PIOCGFAULT	(PIOC|14)	/* get traced fault set */
#define PIOCSFAULT	(PIOC|15)	/* set traced fault set */
#define PIOCCFAULT	(PIOC|16)	/* clear current fault */
#define PIOCGENTRY	(PIOC|17)	/* get syscall entry set */
#define PIOCSENTRY	(PIOC|18)	/* set syscall entry set */
#define PIOCGEXIT	(PIOC|19)	/* get syscall exit set */
#define PIOCSEXIT	(PIOC|20)	/* set syscall exit set */
/*
 * These four are obsolete (replaced by PIOCSET/PIOCRESET)
 */
#define PIOCSFORK	(PIOC|21)	/* set inherit-on-fork flag */
#define PIOCRFORK	(PIOC|22)	/* reset inherit-on-fork flag */
#define PIOCSRLC	(PIOC|23)	/* set run-on-last-close flag */
#define PIOCRRLC	(PIOC|24)	/* reset run-on-last-close flag */

#define PIOCGREG	(PIOC|25)	/* get general registers */
#define PIOCSREG	(PIOC|26)	/* set general registers */
#define PIOCGFPREG	(PIOC|27)	/* get floating-point registers */
#define PIOCSFPREG	(PIOC|28)	/* set floating-point registers */
#define PIOCNICE	(PIOC|29)	/* set nice priority */
#define PIOCPSINFO	(PIOC|30)	/* get ps(1) information */
#define PIOCNMAP	(PIOC|31)	/* get number of memory mappings */
#define PIOCMAP		(PIOC|32)	/* get memory map information */
#define PIOCOPENM	(PIOC|33)	/* open mapped object for reading */
#define PIOCCRED	(PIOC|34)	/* get process credentials */
#define PIOCGROUPS	(PIOC|35)	/* get supplementary groups */
/*
 * These are new with SVR4
 */
#define PIOCSET		(PIOC|38)	/* set modes of operation */
#define PIOCRESET	(PIOC|39)	/* reset modes of operation */
#define PIOCNWATCH	(PIOC|40)	/* get # watch points */
#define PIOCGWATCH	(PIOC|41)	/* get watch point */
#define PIOCSWATCH	(PIOC|42)	/* set watch point */
#define PIOCUSAGE	(PIOC|43)	/* get prusage_t structure */

/* SGI calls */
#define PIOCPTIMERS	(PIOC|100)      /* turn process timers on/off */
#define PIOCGUTID       (PIOC|101)      /* get uthread id(s) */
#define	PIOCGETINODE	(PIOC|102)	/* get inode (fd) information */

#define	PIOCREAD	(PIOC|103)	/* read thread memory */

#ifdef CKPT
/*
 * 180-232 reserved for checkpoint/restart
 */
#endif

/*
 * The following calls manipulate 
 * the R10000 event counters. 
 */
#define PIOCENEVCTRS	(PIOC|233) 	/* acquire and start event counters */
#define PIOCRELEVCTRS	(PIOC|234)	/* release/stop event counters */
#define PIOCGETEVCTRS	(PIOC|235)	/* dump out the counters */
#define PIOCGETPREVCTRS	(PIOC|236)	/* dump counters and prusage info */
#define PIOCGETEVCTRL	(PIOC|237)	/* get control info for counters */
#define PIOCSETEVCTRL	(PIOC|238)	/* set control info for counters */
#define PIOCSAVECCNTRS  (PIOC|239)	/* parent gets child cntrs (no wait) */
#define	PIOCENEVCTRSEX	(PIOC|240)	/* acquire and start event counters */
#define	PIOCGETEVCTRLEX	(PIOC|241)	/* get control info for counters */

#define PIOCPGD_SGI	(PIOC|248)	/* (SGI) get page tbl information */
#define PIOCMAP_SGI	(PIOC|249)	/* (SGI) get region map information */
#define PIOCGETPTIMER	(PIOC|250)	/* get process timers */
#define PIOCTLBMISS	(PIOC|251)	/* turn tlbmiss accounting on/off */
#define PIOCACINFO	(PIOC|252)	/* get current process acct info */

#define PIOCGETSN0REFCNTRS     (PIOC|253)  /* get SN0 page ref cnts*/
#define PIOCGETSN0EXTREFCNTRS  (PIOC|254)  /* get SN0 Soft Ext page ref cnts*/

#define PIOCTHREAD      (PIOC|255)      /* do ioctl on a specific thread. */

/* arguments for PIOCTLBMISS */
#define TLB_STD		0x0
#define TLB_COUNT	0x1

/*
 * Magic cookies to tell procfs that the caller is ready for 64 bit formats
 * even though it was compiled in a 32 bit abi.
 */
#define PIOC_IRIX5_64	(1<<16)
#define PIOC_IRIX5_N32	(1<<17)

#define PRSYSARGS	6		/* max number of syscall arguments */

typedef struct prstatus {
	ulong_t		pr_flags;	/* Process flags */
	short		pr_why;		/* Reason for process stop */
	short		pr_what;	/* More detailed reason */
	short		pr_cursig;	/* Current signal */
	short		pr_nthreads;	/* Number of kernel threads */
	sigset_t	pr_sigpend;	/* Set of pending signals */
	sigset_t	pr_sighold;	/* Set of held signals */
	siginfo_t	pr_info;	/* info assoc. with signal or fault */
	stack_t		pr_altstack;	/* Alternate signal stack info */
	sigaction_t	pr_action;	/* Signal action for current signal */
	long		pr_syscall;	/* syscall number (if in syscall) */
	long		pr_nsysarg;	/* number of arguments to syscall */
	long		pr_errno;	/* error number from system call */
#if (_MIPS_SIM == _ABIN32)
	__int64_t	pr_rval1;	/* syscall return value 1 */
	__int64_t	pr_rval2;	/* syscall return value 2 */
	__int64_t	pr_sysarg[PRSYSARGS];	/* syscall arguments */
#else
	long		pr_rval1;	/* syscall return value 1 */
	long		pr_rval2;	/* syscall return value 2 */
	long		pr_sysarg[PRSYSARGS];	/* syscall arguments */
#endif
	pid_t		pr_pid;		/* Process id */
	pid_t		pr_ppid;	/* Parent process id */
	pid_t		pr_pgrp;	/* Process group id */
	pid_t		pr_sid;		/* Session id */
	timespec_t	pr_utime;	/* Process user cpu time */
	timespec_t	pr_stime;	/* Process system cpu time */
	timespec_t	pr_cutime;	/* Sum of children's user times */
	timespec_t	pr_cstime;	/* Sum of children's system times */
	char		pr_clname[8];	/* Scheduling class name */
	union {
		long	pr_filler[20];	/* Filler area for future expansion */
		struct {
			sigset_t sigpnd;/* Set of signals pending on thread */
			id_t	 who;	/* Which kernel thread */
		} pr_st;
	} pr_un;
	inst_t		pr_instr;	/* Current instruction */
	gregset_t	pr_reg;		/* General registers */
} prstatus_t;
  
#define	pr_thsigpend	pr_un.pr_st.sigpnd
#define	pr_who		pr_un.pr_st.who

/* values for pr_flags */
#define PR_STOPPED	0x0001	/* process is stopped */
#define PR_ISTOP	0x0002	/* process is stopped on event of interest */
#define PR_DSTOP	0x0004	/* process has stop directive in effect */
#define PR_ASLEEP	0x0008	/* process is in an interruptible sleep */
#define PR_FORK		0x0010	/* process has inherit-on-fork flag set. */
#define PR_RLC		0x0020	/* process has run-on-last-close flag set. */
#define PR_PTRACE	0x0040	/* process is traced with ptrace() too */
#define PR_PCINVAL	0x0080	/* current pc is invalid */
#define PR_STEP		0x0200	/* process has single step pending */
#define PR_KLC		0x0400	/* process has kill-on-last-close flag set. */
#define PR_ISKTHREAD	0x1000	/* process is a kernel thread */
#define PR_JOINTSTOP    0x2000  /* process set for stopping of all threads */
                                /* at once. */
#define PR_JOINTPOLL    0x4000  /* all threads of a process must be stopped */
                                /* for poll to return OK, when a joint stop */
                                /* is being effected.  Requires PR_JOINTSTOP */
#define PR_RETURN	0x8000	/* process is about to return to user space */
#define PR_CKF		0x0800	/* process has check fatal sigs-on-last-close flag set. */


/* values for pr_why */
#define PR_REQUESTED	1	/* in the interest of binary compatibility, */
#define PR_SIGNALLED	2	/* PR_REQUESTED thru PR_SYSEXIT match the   */
#define PR_SYSENTRY	3	/* prior settings from proc.h               */
#define PR_SYSEXIT	4
#define PR_FAULTED	5
#define PR_JOBCONTROL	6

typedef struct prrun {
	ulong_t		pr_flags;	/* Flags */
	sigset_t	pr_trace;	/* Set of signals to be traced */
	sigset_t	pr_sighold;	/* Set of signals to be held */
	fltset_t	pr_fault;	/* Set of faults to be traced */
	caddr_t		pr_vaddr;	/* Virt. address at which to resume */
	long		pr_filler[8];	/* Filler area for future expansion */
} prrun_t ;

/* values for pr_flags */
#define PRCSIG		0x0001	/* Clear current signal */
#define PRCFAULT	0x0002	/* Clear current fault */
#define PRSTRACE	0x0004	/* set traced signals from pr_trace */
#define PRSHOLD		0x0008	/* set held signals from pr_sighold */
#define PRSFAULT	0x0010	/* set fault tracing mask from pr_trace */
#define PRSVADDR	0x0020	/* set PC on resumption from pr_vaddr */
#define PRSTEP		0x0040	/* single step the process */
#define PRSABORT	0x0080	/* abort current system call */
#define PRSTOP		0x0100	/* stop as soon as possible */
#define PRCSTEP		0x0200	/* cancel outstanding single step */

#ifdef _KERNEL
#define KPRIFORK	0x0001	/* inherit-on-fork */
#define KPRRLC		0x0002	/* run-on-last-close */
#define KPRSUIDEXEC	0x0008	/* internal flag for secure setuid exec() */
#define KPRKLC		0x0020	/* kill-on-last-close */
#define KPRJSTOP        0x0040  /* stop all threads on debug events */
#define KPRJPOLL        0x0080  /* poll succeeds on joint stop only when */
                                /* all threads are stopped or can make no */
                                /* progress in user space */
#define	KPRCKF		0x0100	/* check for fatal sigs after closing */
#endif

#define PRNODEV		(dev_t)(-1)     /* non-existent device */
#define PRARGSZ		80		/* should match PSARGSZ in user.h */
#define PRCOMSIZ	32		/* should match PSCOMSIZ in user.h */

typedef struct prpsinfo {
	char	pr_state;	/* numeric process state */
	char	pr_sname;	/* printable character representing pr_state */
	char	pr_zomb;	/* !=0: process exited but not waited for */
	char	pr_nice;	/* process priority */
	ulong_t	pr_flag;	/* process flags */
	uid_t	pr_uid;		/* real user id */
	gid_t	pr_gid;		/* real group id */
	pid_t	pr_pid;		/* process id */
	pid_t	pr_ppid;	/* parent process id */
	pid_t	pr_pgrp;	/* pid of process group leader */
	pid_t	pr_sid;		/* session id */
	caddr_t	pr_addr;	/* physical address of process */
	long	pr_size;	/* process image size in pages */
	long	pr_rssize;	/* resident set size in pages */
	caddr_t	pr_wchan;	/* wait addr for sleeping process */
	timespec_t	pr_start;	/* process start time */
	timespec_t	pr_time;	/* usr+sys cpu time for this process */
	long	pr_pri;		/* priority */
	long	pr_oldpri;	/* pre-svr4 priority */
	char	pr_cpu;		/* pre-svr4 cpu usage */
	dev_t	pr_ttydev;	/* controlling tty */
	char	pr_clname[8];	/* scheduling class name */
	char	pr_fname[PRCOMSIZ];	/* basename of exec()'d pathname */
	char	pr_psargs[PRARGSZ];	/* initial chars of arg list */
	uint_t	pr_spare1;	/* spare */
	cpuid_t	pr_sonproc;	/* processor running on */
	timespec_t     pr_ctime;       /* usr+sys cpu time for all children */
	uid_t	pr_shareuid;	/* uid of ShareII Lnode */
	uint_t	pr_spid;	/* sproc pid */
	timespec_t	pr_qtime;	/* cumulative cpu time process */
	char	pr_wname[16];	/* name of lock represented by pr_wchan */
	uint_t	pr_thds;	/* threads */
#if _MIPS_SIM == _ABI64
	int64_t	pr_fill[10];	/* spares */
#else
	int32_t	pr_fill[8];	/* spares */
#endif
} prpsinfo_t;

/* values for pr_flag
 * This used to just be a copy of the p_flag field in the proc structure.
 * However, this is a waste, since ps only looks at the bottom 8 bits, so
 * additional state bits can be defined here to pass out to ps(1) and other
 * programs which make use of the PIOCPSINFO ioctl on the procfs file system.
 */
#define	PR_FLAG_PROC	0x00FF	/* Take these bits from p->p_flag field */
#define PR_FLAG_MASK	0x00FF	/* All bits that can be set in pr_flag */

typedef struct prmap {
	caddr_t	pr_vaddr;	/* Virtual base address */
	ulong_t	pr_size;	/* Size of mapping in bytes */
#if (_MIPS_SIM == _ABIN32)
	off_t		pr_off;	/* Offset into mapped object, if any */
#else
	__scoff_t	pr_off;	/* Offset into mapped object, if any */
#endif
	ulong_t	pr_mflags;	/* Protection and attribute flags */
	long	pr_fill[4];	/* spares */
} prmap_t;

typedef struct prmap_sgi {
	caddr_t	pr_vaddr;	/* Virtual base address */
	ulong_t	pr_size;	/* Size of mapping in bytes */
#if (_MIPS_SIM == _ABIN32)
	off_t	pr_off;		/* Offset into mapped object, if any */
#else
	__scoff_t pr_off;	/* Offset into mapped object, if any */
#endif
	ulong_t	pr_mflags;	/* Protection and attribute flags */
				/* non PYHS type region information... */
	pgno_t	pr_vsize;	/* # of valid pages in this region */
	pgno_t	pr_psize;	/* # of private pages in this region */
	pgno_t	pr_wsize;	/* # of pages in region weighted base 256 */
	pgno_t	pr_rsize;	/* # of referenced pages in this region */
	pgno_t	pr_msize;	/* # of modified pages in this region */
	dev_t	pr_dev;		/* Device # of region iff mapped */
#if ((_MIPS_SIM == _ABIN32) || (_MIPS_SIM == _ABI64) || !defined(_KERNEL))
	ino_t	pr_ino;		/* Inode # of region iff mapped */
#else
	__uint32_t pr_ino;	/* Inode # of region iff mapped */
#endif
	ushort_t pr_lockcnt;	/* lock count */
	char	pr_fill[5*sizeof(long)-sizeof(ushort_t)];	/* spares */
} prmap_sgi_t;

typedef struct prmap_sgi_arg {
	caddr_t	pr_vaddr;	/* Virtual pointer to base of map buffer */
	ulong_t	pr_size;	/* Size of user's map buffer in bytes */
} prmap_sgi_arg_t;

/* flag values in pr_mflags */
#define MA_READ		0x0001	/* mapping has readable permission */
#define MA_WRITE	0x0002	/* mapping has writable permission */
#define MA_EXEC		0x0004	/* mapping has executable permission */
#define MA_SHARED	0x0008	/* mapping is a shared or mapped object */
#define MA_BREAK	0x0010	/* mapping is grown by brk(2) */
#define MA_STACK	0x0020	/* mapping is grown on stack faults */
#define MA_PHYS		0x0040	/* mapping is a physical device */
#define MA_MAPZERO	0x0080	/* mapping contains /dev/zero mappings */
/* flag values added for prmap_sgi */
#define	MA_FETCHOP	0x0400	/* mapping is setup for fetchop */
#define	MA_PRIMARY	0x0800	/* mapping is part of primary object */
#define MA_SREGION	0x1000	/* mapping is on shared region list */
#define MA_COW		0x2000	/* mapping is set to copy on write */
#define	MA_NOTCACHED	0x4000	/* mapping is not cacheable by process */
#define MA_SHMEM	0x8000	/* mapping is a shmem() memory region */
#define	MA_REFCNT_SHIFT	    16	/* shift for region reference count */
/* fractional base for pr_wsize */
#define	MA_WSIZE_FRAC	   256	/* base for pr_wsize */

/* WARNING: pgd_t is the same size for both 32 bit and 64 bit
 * programs -- do not invalidate this assumption.
 */
typedef struct pgd {		/* per-page data */
	short pr_flags;		/* flags */
	short pr_value;		/* page use count /or/ vfault page offset */
} pgd_t;
typedef struct prpgd_sgi {
	caddr_t	pr_vaddr;	/* virtual base address of region to stat */
	pgno_t	pr_pglen;	/* number of pages in data list... */
	pgd_t	pr_data[1];	/* variable length array of page flags */
} prpgd_sgi_t;

/* flag values in prpgd_sgi pr_data.pr_flags */
#define	PGF_REFERENCED	0x0001	/* page is hardware valid in region */
#define	PGF_GLOBAL	0x0002	/* page is hardware global in region */
#define	PGF_WRITEABLE	0x0004	/* page is hardware writeable in region */
#define	PGF_NOTCACHED	0x0008	/* page is not hardware cached in region */
#define	PGF_ISVALID	0x0010	/* page is valid */
#define	PGF_ISDIRTY	0x0020	/* page is dirty */
#define PGF_PRIVATE	0x0040	/* page is private (reference count == 1) */
#define PGF_FAULT	0x0080	/* page fault offset stored pr_value */
#define	PGF_SWAPPED	0x0100	/* page has been swapped in the past */
#define	PGF_ANONBACK	0x0200	/* page is backed by swap */
#define	PGF_USRHISTORY	0x0800	/* history flag reserved for the caller */
#define PGF_REFHISTORY	0x1000	/* has page ever been marked hrdwr valid? */
#define PGF_WRTHISTORY	0x2000	/* has page ever been marked hrdwr dirty? */
#define PGF_VALHISTORY	0x4000	/* has page ever been marked softw valid? */
#define PGF_CLEAR	0x8000	/* clear hardware REF/WRT for this page */
#define	PGF_NONHIST	0xF800	/* mask to clear per call flags */

/* WARNING: prcred_t is the same size for both 32 bit and 64 bit
 * programs -- do not invalidate this assumption.
 */
/* credentials structure */
typedef struct prcred {
	uid_t	pr_euid;	/* Effective user id */
	uid_t	pr_ruid;	/* Real user id */
	uid_t	pr_suid;	/* Saved user id (from exec) */
	gid_t	pr_egid;	/* Effective group id */
	gid_t	pr_rgid;	/* Real group id */
	gid_t	pr_sgid;	/* Saved group id */
	uint_t	pr_ngroups;	/* number of supplementary groups */
} prcred_t;

/* watchpoints structure */
typedef struct prwatch {
	caddr_t pr_vaddr;
	ulong_t	pr_size;
	ulong_t	pr_wflags;
	long	pr_filler;
} prwatch_t;

/* prusage structure */
typedef struct prusage {
	timespec_t pu_tstamp;	/* time stamp */ 
	timespec_t pu_starttime;	/* time process was started */ 
	timespec_t pu_utime;	/* user CPU time */ 
	timespec_t pu_stime;	/* system CPU time */ 
	ulong_t pu_minf;	/* minor (mapping) page faults */
	ulong_t pu_majf;	/* major (disk) page faults */

	ulong_t pu_utlb;	/* user TLB misses */
	ulong_t pu_nswap;	/* swaps (process only) */
	ulong_t pu_gbread;	/* gigabytes ... */ 
	ulong_t pu_bread;	/* and bytes read */
	ulong_t pu_gbwrit;	/* gigabytes ... */ 
	ulong_t pu_bwrit;	/* and bytes written */
	ulong_t pu_sigs;	/* signals received */
	ulong_t pu_vctx;	/* voluntary context switches */
	ulong_t pu_ictx;	/* involuntary context switches */
	ulong_t pu_sysc;	/* system calls */ 
	ulong_t pu_syscr;	/* read() system calls */ 
	ulong_t pu_syscw;	/* write() system calls */ 
	ulong_t pu_syscps;	/* poll() or select() system calls */
	ulong_t pu_sysci;	/* ioctl() system calls */ 
	ulong_t pu_graphfifo;	/* graphics pipeline stalls */
	ulong_t pu_graph_req[8];	/* graphics resource requests */
	ulong_t pu_graph_wait[8];/* graphics resource waits */
	ulong_t pu_size;	/* size of swappable image in pages */
	ulong_t pu_rss;		/* resident set size */
	ulong_t pu_inblock;	/* block input operations */
	ulong_t pu_oublock;	/* block output operations */
	ulong_t pu_vfault;	/* total number of vfaults */
	ulong_t pu_ktlb;	/* kernel TLB misses */
} prusage_t; 

/* pracinfo structure, for PIOCACINFO. Should be same size for */
/* both 32-bit and 64-bit applications. The passing similarity */
/* to the "sat_proc_acct" struct is not accidental.	       */
typedef struct pracinfo {
	char		pr_version;	/* Accounting data version */
	char		pr_flag;	/* Miscellaneous flags */
	char		pr_nice;	/* Nice value */
	uchar_t		pr_sched;	/* Scheduling discipline */
					/* (see sys/schedctl.h) */
	__int32_t	pr_spare1;	/*   reserved */
	ash_t		pr_ash;		/* Array session handle */
	prid_t		pr_prid;	/* Project ID */
	time_t		pr_btime;	/* Begin time (in secs since 1970)*/
	time_t		pr_etime;	/* Elapsed time (in HZ) */
	__int32_t	pr_spare2[2];	/*   reserved */
 	struct acct_timers pr_timers;	/* Assorted timers: see extacct.h */
	struct acct_counts pr_counts;	/* Assorted counters: (ditto) */
	__int64_t	pr_spare3[8];	/*   reserved */
} pracinfo_t;

/* pracinfo.pr_flag */
#define	AIF_FORK	0x80		/* has executed fork, but no exec */
#define	AIF_SU		0x40		/* used privilege */
#define AIF_NOUSER	0x01		/* no user data: some fields invalid */

/* prthreadctl structure, for issuing nested thread-specific ioctls. */
typedef struct prthreadctl {
	tid_t		pt_tid;		/* Id of the designated thread */
        int             pt_cmd;         /* Command value for nested ioctl */
        int             pt_flags;       /* Flags governing use of pt tid */
        caddr_t         pt_data;        /* Data pointer for nested command. */
} prthreadctl_t;

/* values for pt_flags */
#define PTF_DIR		0x0f		/* Flags giving direction. */
#define PTF_SET         0xf0            /* Flags defining set of threads. */

#define PTFD_EQL        0x00            /* Only threads with exact tid. */
#define PTFD_GEQ        0x01            /* Only threads with equal or greater */
                                        /* tid. */
#define PTFD_GTR        0x02            /* Only threads with greater tid. */
#define PTFD_MAX        0x02            /* Max valid direction. */

#define PTFS_ALL        0x00            /* Set includes all threads. */
#define PTFS_STOPPED    0x10            /* Set includes stopped threads. */
#define PTFS_EVENTS     0x20            /* Set includes threads with new */
                                        /* events. */
#define PTFS_MAX        0x20            /* Max valid set. */

/* prinodeinfo structure, for PIOCGETINODE */
typedef struct prinodeinfo {
	int		pi_fd;		/* input: process' file descriptor */
	dev_t		pi_dev;		/* output: filesystem device for file */
	ino64_t		pi_inum;	/* output: inode number of file */
	uint32_t	pi_gen;		/* output: generation num for inode */
} prinodeinfo_t;

/* values for pr_wflags - see MA_* */

/* prio structure for PIOCREAD */
typedef struct prio {
	off64_t		pi_offset;	/* offset into target proc */
	void		*pi_base;	/* address of callers buffer */
	ssize_t		pi_len;		/* io length */
} prio_t;

/*
 * Macros for manipulating sets of flags.
 * sp must be a pointer to one of sigset_t, fltset_t, or sysset_t.
 * flag must be a member of the enumeration corresponding to *sp.
 */

/* turn on all flags in set */
#define prfillset(sp) \
	{ register int _i_ = sizeof(*(sp))/sizeof(__uint32_t); \
		while(_i_) ((__uint32_t*)(sp))[--_i_] = 0xFFFFFFFF; }

/* turn off all flags in set */
#define premptyset(sp) \
	{ register int _i_ = sizeof(*(sp))/sizeof(__uint32_t); \
		while(_i_) ((__uint32_t*)(sp))[--_i_] = 0L; }

/* is this set empty? */
#define prisemptyset(set) \
	(setisempty(set, sizeof(*(set))/sizeof(__uint32_t)))

/* turn on specified flag in set */
#define praddset(sp, flag) \
	(((unsigned)((flag)-1) < 32*sizeof(*(sp))/sizeof(__uint32_t)) \
	&& (((__uint32_t*)(sp))[((flag)-1)/32] |= (1L<<(((flag)-1)%32))))

/* turn off specified flag in set */
#define prdelset(sp, flag) \
	(((unsigned)((flag)-1) < 32*sizeof(*(sp))/sizeof(__uint32_t)) \
	&& (((__uint32_t*)(sp))[((flag)-1)/32] &= ~(1L<<(((flag)-1)%32))))

/* query: != 0 iff flag is turned on in set */
#define prismember(sp, flag) \
	(((unsigned)((flag)-1) < 32*sizeof(*(sp))/sizeof(__uint32_t)) \
	&& (((__uint32_t*)(sp))[((flag)-1)/32] & (1L<<(((flag)-1)%32))))


#endif	/* _FS_PROCFS_PROCFS_H */
