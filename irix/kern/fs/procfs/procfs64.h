/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef _FS_PROCFS_PROCFS64_H	/* wrapper symbol for kernel use */
#define _FS_PROCFS_PROCFS64_H	/* subject to change without notice */

#ident	"$Id: procfs64.h,v 1.17 1997/10/03 20:56:16 jph Exp $"

#include <sys/fault.h>
#include <sys/siginfo.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/ucontext.h>
#include <sys/procfs.h>
#include <sys/ktime.h>
#include <sys/kucontext.h>

#if _KMEMUSER

/*
 * ioctl codes and system call interfaces for /proc.
 * These allow 32 bit programs to access 64 bit program info.
 */
#define PIOC64		(PIOC_IRIX5_64 | PIOC)
#define IRIX5_64_PIOCSTATUS	(PIOC64|1)	/* get process status */
#define IRIX5_64_PIOCSTOP	(PIOC64|2)	/* post STOP request and... */
#define IRIX5_64_PIOCWSTOP	(PIOC64|3)	/* wait for process to STOP */
#define IRIX5_64_PIOCRUN	(PIOC64|4)	/* make process runnable */
#define IRIX5_64_PIOCGTRACE	(PIOC64|5)	/* get traced signal set */
#define IRIX5_64_PIOCSTRACE	(PIOC64|6)	/* set traced signal set */
#define IRIX5_64_PIOCSSIG	(PIOC64|7)	/* set current signal */
#define IRIX5_64_PIOCKILL	(PIOC64|8)	/* send signal */
#define IRIX5_64_PIOCUNKILL	(PIOC64|9)	/* delete a signal */
#define IRIX5_64_PIOCGHOLD	(PIOC64|10)	/* get held signal set */
#define IRIX5_64_PIOCSHOLD	(PIOC64|11)	/* set held signal set */
#define IRIX5_64_PIOCMAXSIG	(PIOC64|12)	/* get max signal number */
#define IRIX5_64_PIOCACTION	(PIOC64|13)	/* get signal action structs */
#define IRIX5_64_PIOCGFAULT	(PIOC64|14)	/* get traced fault set */
#define IRIX5_64_PIOCSFAULT	(PIOC64|15)	/* set traced fault set */
#define IRIX5_64_PIOCCFAULT	(PIOC64|16)	/* clear current fault */
#define IRIX5_64_PIOCGENTRY	(PIOC64|17)	/* get syscall entry set */
#define IRIX5_64_PIOCSENTRY	(PIOC64|18)	/* set syscall entry set */
#define IRIX5_64_PIOCGEXIT	(PIOC64|19)	/* get syscall exit set */
#define IRIX5_64_PIOCSEXIT	(PIOC64|20)	/* set syscall exit set */
/*
 * These four are obsolete (replaced by PIOCSET/PIOCRESET)
 */
#define IRIX5_64_PIOCSFORK	(PIOC64|21)	/* set inherit-on-fork flag */
#define IRIX5_64_PIOCRFORK	(PIOC64|22)	/* reset inherit-on-fork flag */
#define IRIX5_64_PIOCSRLC	(PIOC64|23)	/* set run-on-last-close flag */
#define IRIX5_64_PIOCRRLC	(PIOC64|24)	/* reset run-on-last-close flag */

#define IRIX5_64_PIOCGREG	(PIOC64|25)	/* get general registers */
#define IRIX5_64_PIOCSREG	(PIOC64|26)	/* set general registers */
#define IRIX5_64_PIOCGFPREG	(PIOC64|27)	/* get floating-point registers */
#define IRIX5_64_PIOCSFPREG	(PIOC64|28)	/* set floating-point registers */
#define IRIX5_64_PIOCNICE	(PIOC64|29)	/* set nice priority */
#define IRIX5_64_PIOCPSINFO	(PIOC64|30)	/* get ps(1) information */
#define IRIX5_64_PIOCNMAP	(PIOC64|31)	/* get number of memory mappings */
#define IRIX5_64_PIOCMAP	(PIOC64|32)	/* get memory map information */
#define IRIX5_64_PIOCOPENM	(PIOC64|33)	/* open mapped object for reading */
#define IRIX5_64_PIOCCRED	(PIOC64|34)	/* get process credentials */
#define IRIX5_64_PIOCGROUPS	(PIOC64|35)	/* get supplementary groups */
#define IRIX5_64_PIOCGETPR	(PIOC64|36)	/* read struct proc */
#define IRIX5_64_PIOCGETU	(PIOC64|37)	/* read user area */
/*
 * These are new with SVR4
 */
#define IRIX5_64_PIOCSET	(PIOC64|38)	/* set modes of operation */
#define IRIX5_64_PIOCRESET	(PIOC64|39)	/* reset modes of operation */
#define IRIX5_64_PIOCNWATCH	(PIOC64|40)	/* get # watch points */
#define IRIX5_64_PIOCGWATCH	(PIOC64|41)	/* get watch point */
#define IRIX5_64_PIOCSWATCH	(PIOC64|42)	/* set watch point */
#define IRIX5_64_PIOCUSAGE	(PIOC64|43)	/* get prusage_t structure */

/* SGI calls */
#define IRIX5_64_PIOCPGD_SGI	(PIOC64|248)	/* (SGI) get page tbl information */
#define IRIX5_64_PIOCMAP_SGI	(PIOC64|249)	/* (SGI) get region map information */
#define IRIX5_64_PIOCGETPTIMER	(PIOC64|250)	/* get process timers */

typedef struct irix5_64_prstatus {
	app64_ulong_t	pr_flags;	/* Process flags */
	short		pr_why;		/* Reason for process stop */
	short		pr_what;	/* More detailed reason */
	short		pr_cursig;	/* Current signal */
	short		pr_nthreads;	/* Number of kernel threads */
	sigset_t	pr_sigpend;	/* Set of pending signals */
	sigset_t	pr_sighold;	/* Set of held signals */
	struct irix5_64_siginfo	pr_info;/* info assoc. with signal or fault */
	struct irix5_64_sigaltstack pr_altstack;/*Alternate signal stack info */
	irix5_64_sigaction_t	pr_action;/* Signal action for current signal */
	app64_long_t	pr_syscall;	/* syscall number (if in syscall) */
	app64_long_t	pr_nsysarg;	/* number of arguments to syscall */
	app64_long_t	pr_errno;	/* error number from system call */
	app64_long_t	pr_rval1;	/* syscall return value 1 */
	app64_long_t	pr_rval2;	/* syscall return value 2 */
	app64_long_t	pr_sysarg[PRSYSARGS];	/* syscall arguments */
	pid_t		pr_pid;		/* Process id */
	pid_t		pr_ppid;	/* Parent process id */
	pid_t		pr_pgrp;	/* Process group id */
	pid_t		pr_sid;		/* Session id */
	irix5_64_timespec_t	pr_utime;/* Process user cpu time */
	irix5_64_timespec_t	pr_stime;/* Process system cpu time */
	irix5_64_timespec_t	pr_cutime;/* Sum of children's user times */
	irix5_64_timespec_t	pr_cstime;/* Sum of children's system times */
	char		pr_clname[8];	/* Scheduling class name */
	union {
		app64_long_t	pr_filler[20];	/* Filler area */
		struct {
			sigset_t sigpnd;/* Set of signals pending on thread */
			id_t	 who;	/* Which kernel thread */
		} pr_st;
	} pr_un;
	inst_t		pr_instr;	/* Current instruction */
	irix5_64_gregset_t	pr_reg;	/* General registers */
} irix5_64_prstatus_t;
  
#define	pr_thsigpend	pr_un.pr_st.sigpnd
#define	pr_who		pr_un.pr_st.who

typedef struct irix5_64_prrun {
	app64_ulong_t	pr_flags;	/* Flags */
	sigset_t	pr_trace;	/* Set of signals to be traced */
	sigset_t	pr_sighold;	/* Set of signals to be held */
	fltset_t	pr_fault;	/* Set of faults to be traced */
	app64_ptr_t	pr_vaddr;	/* Virt. address at which to resume */
	app64_long_t	pr_filler[8];	/* Filler area for future expansion */
} irix5_64_prrun_t ;

typedef struct irix5_64_prpsinfo {
	char	pr_state;	/* numeric process state */
	char	pr_sname;	/* printable character representing pr_state */
	char	pr_zomb;	/* !=0: process exited but not waited for */
	char	pr_nice;	/* process priority */
	app64_ulong_t	pr_flag;	/* process flags */
	uid_t	pr_uid;		/* real user id */
	gid_t	pr_gid;		/* real group id */
	pid_t	pr_pid;		/* process id */
	pid_t	pr_ppid;	/* parent process id */
	pid_t	pr_pgrp;	/* pid of process group leader */
	pid_t	pr_sid;		/* session id */
	app64_ptr_t	pr_addr;/* physical address of process */
	app64_long_t pr_size;	/* process image size in pages */
	app64_long_t pr_rssize;	/* resident set size in pages */
	app64_ptr_t	pr_wchan;/* wait addr for sleeping process */
	irix5_64_timespec_t	pr_start;/* process start time */
	irix5_64_timespec_t	pr_time;/* usr+sys cpu time for this process */
	app64_long_t pr_pri;		/* priority */
	app64_long_t pr_oldpri;	/* pre-svr4 priority */
	char	pr_cpu;		/* pre-svr4 cpu usage */
	dev_t	pr_ttydev;	/* controlling tty */
	char	pr_clname[8];	/* scheduling class name */
	char	pr_fname[PRCOMSIZ];	/* basename of exec()'d pathname */
	char	pr_psargs[PRARGSZ];	/* initial chars of arg list */
	uint_t	pr_spare1;	/* spare */
	cpuid_t	pr_sonproc;	/* processor running on */
	irix5_64_timespec_t    pr_ctime; /* usr+sys time for all children */
	uid_t	pr_shareuid;	/* uid of ShareII Lnode */
	app64_int_t pr_spid;	/* threads */
	irix5_64_timespec_t	pr_qtime;/* cumulative cpu time process */
	app64_int_t pr_fill2;	/* spare */
	app64_long_t pr_fill[14];	/* spares */
} irix5_64_prpsinfo_t;

typedef struct irix5_64_prmap {
	app64_ptr_t	pr_vaddr;	/* Virtual base address */
	app64_ulong_t	pr_size;	/* Size of mapping in bytes */
	off64_t		pr_off;		/* Offset into mapped object, if any */
	app64_ulong_t	pr_mflags;	/* Protection and attribute flags */
	app64_long_t	pr_fill[4];	/* spares */
} irix5_64_prmap_t;

typedef struct irix5_64_prmap_sgi {
	app64_ptr_t	pr_vaddr;	/* Virtual base address */
	app64_ulong_t	pr_size;	/* Size of mapping in bytes */
	off64_t		pr_off;		/* Offset into mapped object, if any */
	app64_ulong_t	pr_mflags;	/* Protection and attribute flags */
					/* non PYHS type region information... */
	app64_long_t	pr_vsize;	/* # of valid pages in this region */
	app64_long_t	pr_psize;	/* # of private pages in this region */
	app64_long_t	pr_wsize;	/* # of pages in region weighted base 256 */
	app64_long_t	pr_rsize;	/* # of referenced pages in this region */
	app64_long_t	pr_msize;	/* # of modified pages in this region */
	dev_t	pr_dev;			/* Device # of region iff mapped */
	app64_ulong_t	pr_ino;		/* Inode # of region iff mapped */
	app64_long_t pr_fill[5];	/* spares */
} irix5_64_prmap_sgi_t;

typedef struct irix5_64_prmap_sgi_arg {
	app64_ptr_t	pr_vaddr;	/* Virtual pointer to base of map buffer */
	app64_ulong_t	pr_size;	/* Size of user's map buffer in bytes */
} irix5_64_prmap_sgi_arg_t;

typedef struct irix5_64_prpgd_sgi {
	app64_ptr_t	pr_vaddr;	/* virtual base address of region to stat */
	app64_long_t	pr_pglen;	/* number of pages in data list... */
	pgd_t	pr_data[1];		/* variable length array of page flags */
} irix5_64_prpgd_sgi_t;

/* watchpoints structure */
typedef struct irix5_64_prwatch {
	app64_ptr_t	pr_vaddr;
	app64_ulong_t	pr_size;
	app64_ulong_t	pr_wflags;
	app64_long_t	pr_filler;
} irix5_64_prwatch_t;

/* prusage structure */
typedef struct irix5_64_prusage {
	irix5_64_timespec_t pu_tstamp;	/* time stamp */ 
	irix5_64_timespec_t pu_starttime;	/* time process was started */ 
	irix5_64_timespec_t pu_utime;	/* user CPU time */ 
	irix5_64_timespec_t pu_stime;	/* system CPU time */ 
	app64_ulong_t pu_minf;		/* minor (mapping) page faults */
	app64_ulong_t pu_majf;		/* major (disk) page faults */
	app64_ulong_t pu_utlb;		/* user TLB misses */
	app64_ulong_t pu_nswap;		/* swaps (process only) */
	app64_ulong_t pu_gbread;	/* gigabytes ... */ 
	app64_ulong_t pu_bread;		/* and bytes read */
	app64_ulong_t pu_gbwrit;	/* gigabytes ... */ 
	app64_ulong_t pu_bwrit;		/* and bytes written */
	app64_ulong_t pu_sigs;		/* signals received */
	app64_ulong_t pu_vctx;		/* voluntary context switches */
	app64_ulong_t pu_ictx;		/* involuntary context switches */
	app64_ulong_t pu_sysc;		/* system calls */ 
	app64_ulong_t pu_syscr;		/* read() system calls */ 
	app64_ulong_t pu_syscw;		/* write() system calls */ 
	app64_ulong_t pu_syscps;	/* poll() or select() system calls */
	app64_ulong_t pu_sysci;		/* ioctl() system calls */ 
	app64_ulong_t pu_graphfifo;	/* graphics pipeline stalls */
	app64_ulong_t pu_graph_req[8];	/* graphics resource requests */
	app64_ulong_t pu_graph_wait[8];	/* graphics resource waits */
	app64_ulong_t pu_size;		/* size of swappable image in pages */
	app64_ulong_t pu_rss;		/* resident set size */
	app64_ulong_t pu_inblock;	/* block input operations */
	app64_ulong_t pu_oublock;	/* block output operations */
	app64_ulong_t pu_vfault;	/* total number of vfaults */
	app64_ulong_t pu_ktlb;		/* kernel TLB misses */
} irix5_64_prusage_t; 
#endif /* _KMEMUSER */
#endif	/* _FS_PROCFS_PROCFS64_H */
