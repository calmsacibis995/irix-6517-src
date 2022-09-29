#ifndef __PROCFS_N32_H__
#define __PROCFS_N32_H__
/*
 * procfs_n32.h
 *
 *	n32 sized proc structures
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

#ident "$Revision: 1.6 $"
#include <sys/fault.h>
#include <sys/siginfo.h>
#include <sys/signal.h>
#include <sys/ksignal.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/ucontext.h>
#include <sys/procfs.h>
#include <sys/ktime.h>
#include <sys/kucontext.h>

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#if defined(_KMEMUSER) || defined(_KERNEL)
/*
 * ioctl codes and system call interfaces for /proc.
 * These allow 32 bit programs to access n32 program info.
 */
#define PIOCN32		(PIOC_IRIX5_N32 | PIOC)
#define IRIX5_N32_PIOCSTATUS	(PIOCN32|1)	/* get process status */
#define IRIX5_N32_PIOCSTOP	(PIOCN32|2)	/* post STOP request and... */
#define IRIX5_N32_PIOCWSTOP	(PIOCN32|3)	/* wait for process to STOP */
#define IRIX5_N32_PIOCRUN	(PIOCN32|4)	/* make process runnable */
#define IRIX5_N32_PIOCGTRACE	(PIOCN32|5)	/* get traced signal set */
#define IRIX5_N32_PIOCSTRACE	(PIOCN32|6)	/* set traced signal set */
#define IRIX5_N32_PIOCSSIG	(PIOCN32|7)	/* set current signal */
#define IRIX5_N32_PIOCKILL	(PIOCN32|8)	/* send signal */
#define IRIX5_N32_PIOCUNKILL	(PIOCN32|9)	/* delete a signal */
#define IRIX5_N32_PIOCGHOLD	(PIOCN32|10)	/* get held signal set */
#define IRIX5_N32_PIOCSHOLD	(PIOCN32|11)	/* set held signal set */
#define IRIX5_N32_PIOCMAXSIG	(PIOCN32|12)	/* get max signal number */
#define IRIX5_N32_PIOCACTION	(PIOCN32|13)	/* get signal action structs */
#define IRIX5_N32_PIOCGFAULT	(PIOCN32|14)	/* get traced fault set */
#define IRIX5_N32_PIOCSFAULT	(PIOCN32|15)	/* set traced fault set */
#define IRIX5_N32_PIOCCFAULT	(PIOCN32|16)	/* clear current fault */
#define IRIX5_N32_PIOCGENTRY	(PIOCN32|17)	/* get syscall entry set */
#define IRIX5_N32_PIOCSENTRY	(PIOCN32|18)	/* set syscall entry set */
#define IRIX5_N32_PIOCGEXIT	(PIOCN32|19)	/* get syscall exit set */
#define IRIX5_N32_PIOCSEXIT	(PIOCN32|20)	/* set syscall exit set */
/*
 * These four are obsolete (replaced by PIOCSET/PIOCRESET)
 */
#define IRIX5_N32_PIOCSFORK	(PIOCN32|21)	/* set inherit-on-fork flag */
#define IRIX5_N32_PIOCRFORK	(PIOCN32|22)	/* reset inherit-on-fork flag */
#define IRIX5_N32_PIOCSRLC	(PIOCN32|23)	/* set run-on-last-close flag */
#define IRIX5_N32_PIOCRRLC	(PIOCN32|24)	/* reset run-on-last-close flag */

#define IRIX5_N32_PIOCGREG	(PIOCN32|25)	/* get general registers */
#define IRIX5_N32_PIOCSREG	(PIOCN32|26)	/* set general registers */
#define IRIX5_N32_PIOCGFPREG	(PIOCN32|27)	/* get floating-point registers */
#define IRIX5_N32_PIOCSFPREG	(PIOCN32|28)	/* set floating-point registers */
#define IRIX5_N32_PIOCNICE	(PIOCN32|29)	/* set nice priority */
#define IRIX5_N32_PIOCPSINFO	(PIOCN32|30)	/* get ps(1) information */
#define IRIX5_N32_PIOCNMAP	(PIOCN32|31)	/* get number of memory mappings */
#define IRIX5_N32_PIOCMAP	(PIOCN32|32)	/* get memory map information */
#define IRIX5_N32_PIOCOPENM	(PIOCN32|33)	/* open mapped object for reading */
#define IRIX5_N32_PIOCCRED	(PIOCN32|34)	/* get process credentials */
#define IRIX5_N32_PIOCGROUPS	(PIOCN32|35)	/* get supplementary groups */
#define IRIX5_N32_PIOCGETPR	(PIOCN32|36)	/* read struct proc */
#define IRIX5_N32_PIOCGETU	(PIOCN32|37)	/* read user area */
/*
 * These are new with SVR4
 */
#define IRIX5_N32_PIOCSET	(PIOCN32|38)	/* set modes of operation */
#define IRIX5_N32_PIOCRESET	(PIOCN32|39)	/* reset modes of operation */
#define IRIX5_N32_PIOCNWATCH	(PIOCN32|40)	/* get # watch points */
#define IRIX5_N32_PIOCGWATCH	(PIOCN32|41)	/* get watch point */
#define IRIX5_N32_PIOCSWATCH	(PIOCN32|42)	/* set watch point */
#define IRIX5_N32_PIOCUSAGE	(PIOCN32|43)	/* get prusage_t structure */

/* SGI calls */
#define IRIX5_N32_PIOCPGD_SGI	(PIOCN32|248)	/* (SGI) get page tbl information */
#define IRIX5_N32_PIOCMAP_SGI	(PIOCN32|249)	/* (SGI) get region map information */
#define IRIX5_N32_PIOCGETPTIMER	(PIOCN32|250)	/* get process timers */

typedef struct irix5_n32_prstatus {
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
	app32_long_long_t	pr_rval1;	/* syscall return value 1 */
	app32_long_long_t	pr_rval2;	/* syscall return value 2 */
	app32_long_long_t	pr_sysarg[PRSYSARGS];	/* syscall arguments */
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
		/* Filler area for future expansion */
		app32_long_t	pr_filler[20];
		struct {
			sigset_t sigpnd;/* Set of signals pending on thread */
			id_t	 who;	/* Which kernel thread */
		} pr_st;
	} pr_un;
	inst_t		pr_instr;	/* Current instruction */
	irix5_64_gregset_t	pr_reg;		/* General registers */
} irix5_n32_prstatus_t;

#define	pr_thsigpend	pr_un.pr_st.sigpnd
#define	pr_who		pr_un.pr_st.who

typedef struct irix5_n32_prmap {
	app32_ptr_t	pr_vaddr;	/* Virtual base address */
	app32_ulong_t	pr_size;	/* Size of mapping in bytes */
	irix5_n32_off_t	pr_off;		/* Offset into mapped object, if any */
	app32_ulong_t	pr_mflags;	/* Protection and attribute flags */
	app32_long_t	pr_filler[4];	/* for future expansion */
} irix5_n32_prmap_t ;

typedef struct irix5_n32_prmap_sgi {
	app32_ptr_t	pr_vaddr;	/* Virtual base address */
	app32_ulong_t	pr_size;	/* Size of mapping in bytes */
	irix5_n32_off_t	pr_off;		/* Offset into mapped object, if any */
	app32_ulong_t	pr_mflags;	/* Protection and attribute flags */
	app32_ulong_t	pr_vsize;	/* # of valid pages in region */
	app32_ulong_t	pr_psize;	/* # of private pages in this region */
	app32_ulong_t	pr_wsize;	/* usage counts for valid pages  */
	app32_ulong_t	pr_rsize;       /* # of referenced pages */
	app32_ulong_t	pr_msize;       /* # of modified pages */
	app32_ulong_t	pr_dev;		/* Device # of region iff mapped */
	app32_ulong_long_t pr_ino;	/* Inode # of region iff mapped */
	ushort_t	pr_lockcnt;	/* lock count */
	char		pr_filler[5*sizeof(app32_long_t)-sizeof(ushort_t)];
					/* for future expansion */
} irix5_n32_prmap_sgi_t ;

#endif /* _KMEMUSER || _KERNEL */

#endif /* C || C++ */

#endif /* !__PROCFS_N32_H__ */
