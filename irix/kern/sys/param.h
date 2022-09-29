/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 * 
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 * 
 * 
 * 
 * 		Copyright Notice 
 * 
 * Notice of copyright on this source code product does not indicate 
 * publication.
 * 
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *  
 */

#ifndef _SYS_PARAM_H
#define _SYS_PARAM_H

#ident	"$Revision: 3.90 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#include <sys/types.h> 
#include <sys/signal.h> 	/* BSD compat */
#endif

/*
 * Fundamental variables; don't change too often.
 * Warning: keep in sync with <unistd.h>, <sys/unistd.h>
 */

/* POSIX version number, returned by sysconf() system call */
#ifndef _POSIX_VERSION
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#define _POSIX_VERSION	199506L
#endif

#if defined(_LANGUAGE_FORTRAN)
#define _POSIX_VERSION 199506
#endif
#endif

#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE 0 /* Disable special character functions */
#endif

#ifndef MAX_INPUT
#define MAX_INPUT 512     /* Maximum bytes stored in the input queue */
#endif

#ifndef MAX_CANON
#define MAX_CANON 256     /* Maximum bytes in a line for canoical processing */
#endif

#define UID_NOBODY	60001	/* user ID no body */
#define GID_NOBODY	UID_NOBODY

#define UID_NOACCESS	60002	/* user ID no access */

/* NOTE that MAXPID is an internal limit, and is not necessarily the
 * same as PID_MAX, defined in <limits.h>
 * The procfs readdir algorithm depends on this value being somewhat less
 * than MAXINT.
 */
#define	MAXPID	0x7ffffff0	/* max process id */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#define	MAXUID	0x7fffffff	/* max user id */
#endif
#if defined(_LANGUAGE_FORTRAN)
#define MAXUID  $7fffffff
#endif
#define	MAXLINK	30000		/* max links */

#define	SSIZE	1		/* initial stack size (*NBPP bytes) */
#define	SINCR	1		/* increment of stack (*NBPP bytes) */

#define KSTKSIZE	1	/* Number of pages for kernel stack */

#if	_PAGESZ == 4096
#define	EXTKSTKSIZE	1	/* size of kern stack extension (*NBPP bytes) */
#define KSTKIDX		0
#define KSTEIDX		1	/* Kernel stack extension index */
#else	/* _PAGESZ == 16384 */
#define	EXTKSTKSIZE	0	/* size of kern stack extension (*NBPP bytes) */
#define KSTKIDX		0
#endif	/* _PAGESZ == 16384 */

#define	CANBSIZ	256		/* max size of typewriter line	*/
#define	HZ	100		/* 100 ticks/second of the clock */
#define TICK    10000000	/* nanoseconds per tick */

#define NOFILE	20		/* this define is here for	*/
				/* compatibility purposes only	*/
				/* and will be removed in a	*/
				/* later release		*/

/*
 * These define the maximum and minimum allowable values of the
 * configurable parameter NGROUPS_MAX.
 */
#define	NGROUPS_UMIN	0
#define	NGROUPS_UMAX	32

/* 
 * NGROUPS must not be set greater than NGROUPS_MAX in master.d/kernel.
 * Compilation will not succeed in that case.
 */
#define NGROUPS		16	/* max # groups process may be in */

/*
 * Priorities.  Should not be altered too much.
 */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#define	PMASK	0177
#define	PCATCH	0400
#define	PLTWAIT 01000		/* Long term wait */
#define	PRECALC 01000		/* Keep other kernel isms happy for now */
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */
#if defined(_LANGUAGE_FORTRAN)
#define	PMASK	O'0177'
#define	PCATCH	O'0400'
#define	PLTWAIT O'01000'
#define	PRECALC O'01000'
#endif  /* defined(_LANGUAGE_FORTRAN) */
#define	PSWP	0
#define	PINOD	10
#define	PSNDD	PINOD
#define	PRIBIO	20
#define	PZERO	25
#define PMEM	0
#ifndef NZERO
#define	NZERO	20
#endif
#define	PPIPE	26
#define	PVFS	27
#define	PWAIT	30
#define	PSLEP	39
#define	PUSER	60

/*
 * extended rt pri range
 */
#define PBATCH_CRITICAL -1
#define PTIME_SHARE	-2
#define PTIME_SHARE_OVER	-3
#define PBATCH		-4
#define PWEIGHTLESS	-5
/* for NDPLOMAX and NDPLOMIN defs include <sys/schedctl.h> */
#define	PIDLE		(PWEIGHTLESS + NDPLOMAX - NDPLOMIN - 1)

/*
 * fundamental constants of the implementation--
 * cannot be changed easily
 */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#define NBPS	(NCPS*NBPC)	/* Number of bytes per segment */
#define	NBPW	sizeof(int)	/* number of bytes in an integer */
#define	NCPS	(NBPC/(sizeof(pte_t)))	/* Number of clicks per segment */
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */


#define	IO_NBPC		4096	/* Number of bytes per click for DMA purposes */
#define	IO_BPCSHIFT	12	/* LOG2(IO_NBPC) if exact */

#define	MIN_NBPC	4096	/* Minimum number of bytes per click */
#define	MIN_BPCSHIFT	12	/* LOG2(MIN_NBPC) if exact */
#define MIN_CPSSHIFT	10	/* LOG2(MIN_NCPS) if exact */

#define	NBPC		_PAGESZ	/* Number of bytes per click */

#if	NBPC == 4096
#define	BPCSHIFT	12	/* LOG2(NBPC) if exact */
#define CPSSHIFT	10	/* LOG2(NCPS) if exact */
#endif
#if	NBPC == 16384
#define	BPCSHIFT	14	/* LOG2(NBPC) if exact */
#ifndef	PTE_64BIT
#define CPSSHIFT	12	/* LOG2(NCPS) if exact */
#else	/* PTE_64BIT */
#define CPSSHIFT	11	/* LOG2(NCPS) if exact */
#endif	/* PTE_64BIT */
#endif

#define BPSSHIFT	(BPCSHIFT+CPSSHIFT)

#ifndef NULL
#define	NULL	0L
#endif
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#define	CMASK	022		/* default mask for file creation */
#define	NODEV	(dev_t)(-1)
#define NOPAGE	((unsigned int)-1)
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */
#if defined(_LANGUAGE_FORTRAN)
#define	CMASK	O'022'
#define	NODEV	(-1)
#define NOPAGE	(-1)
#endif  /* defined(_LANGUAGE_FORTRAN) */

/*
 * XXX These should be expunged, and BBSHIFT should be defined as 9.
 */
#define	NBPSCTR		512	/* Bytes per disk sector.	*/
#define SCTRSHFT	9	/* Shift for BPSECT.		*/

/* in mips the psw is the status register */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#if IP32
#define	BASEPRI(psw)	(((psw) & SR_IMASK) == SR_IMASK0)
#else
#define	BASEPRI(psw)	(((psw) & SR_IMASK) == SR_IMASK)
#endif

#define	USERMODE(psw)	(((psw) & SR_KSU_MSK) == SR_KSU_USR)

#if !defined(_LANGUAGE_ASSEMBLY)
struct paramconst {
	int		p_usize;	/* KSTKSIZE */
	int		p_extusize;	/* EXTKSTKSIZE */
};
#endif

#ifdef _MIPSEB
#define	lobyte(X)	(((unsigned char *)&X)[1])
#define	hibyte(X)	(((unsigned char *)&X)[0])
#define	loword(X)	(((ushort *)&X)[1])
#define	hiword(X)	(((ushort *)&X)[0])
#else
#define	lobyte(X)	(((unsigned char *)&X)[0])
#define	hibyte(X)	(((unsigned char *)&X)[1])
#define	loword(X)	(((ushort *)&X)[0])
#define	hiword(X)	(((ushort *)&X)[1])
#endif
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */

/*
 * MAXPATHLEN defines the longest permissible path length,
 * including the terminating null, after expanding symbolic links.
 * MAXSYMLINKS defines the maximum number of symbolic links
 * that may be expanded in a path name. It should be set high
 * enough to allow all legitimate uses, but halt infinite loops
 * reasonably quickly.
 * MAXNAMELEN is the length (including the terminating null) of
 * the longest permissible file (component) name.
 */
#define	MAXPATHLEN	1024
#define	MAXSYMLINKS	30
#define	MAXNAMELEN	256

/*
 * The following are defined to be the same as
 * defined in /usr/include/limits.h.  They are
 * needed for pipe and FIFO compatibility.
 */
#ifndef PIPE_BUF	/* max # bytes atomic in write to a pipe */
#define PIPE_BUF	10240
#endif	/* PIPE_BUF */

#ifndef PIPE_MAX	/* max # bytes written to a pipe in a write */
#define PIPE_MAX	10240
#endif	/* PIPE_MAX */

#ifndef	NBBY
#define	NBBY	8	/* number of bits per byte */
#endif	/* NBBY */

/*
 * Block I/O parameterization.  A basic block (BB) is the lowest size of
 * filesystem allocation, and must == NBPSCTR.  Length units given to bio
 * routines are in BB's.
 */
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#define	BBSHIFT		9
#define	BBSIZE		(1<<BBSHIFT)
#define	BBMASK		(BBSIZE-1)
#define	BTOBB(bytes)	(((unsigned long)(bytes) + BBSIZE - 1) >> BBSHIFT)
#define	BTOBBT(bytes)	((unsigned long)(bytes) >> BBSHIFT)
#define	BBTOB(bbs)	((bbs) << BBSHIFT)
#define OFFTOBB(bytes)	(((__uint64_t)(bytes) + BBSIZE - 1) >> BBSHIFT)
#define	OFFTOBBT(bytes)	((off_t)(bytes) >> BBSHIFT)
#define	BBTOOFF(bbs)	((off_t)(bbs) << BBSHIFT)     
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */
     
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#define SEEKLIMIT32	0x7fffffff
#define BBSEEKLIMIT32	BTOBBT(SEEKLIMIT32)
#define SEEKLIMIT	0x7fffffffffffffffLL
#define BBSEEKLIMIT	OFFTOBBT(SEEKLIMIT)     
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */
#if defined(_LANGUAGE_FORTRAN)
#define SEEKLIMIT32	$7fffffff
#define SEEKLIMIT	$7fffffffffffffff
#endif  /* defined(_LANGUAGE_FORTRAN) */


/*
 * File system parameters and macros.
 *
 * The file system is made out of blocks of at most MAXBSIZE units,
 * with smaller units (fragments) only in the last direct block.
 * MAXBSIZE primarily determines the size of buffers in the buffer
 * pool. It may be made larger without any effect on existing
 * file systems; however making it smaller make make some file
 * systems unmountable.
 *
 * Note that the blocked devices are assumed to have DEV_BSIZE
 * "sectors" and that fragments must be some multiple of this size.
 */
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#define	MAXBSIZE	8192
#define	DEV_BSIZE	BBSIZE
#define	DEV_BSHIFT	BBSHIFT		/* log2(DEV_BSIZE) */

#define	btodb(bytes)	 		/* calculates (bytes / DEV_BSIZE) */ \
	((unsigned)(bytes) >> DEV_BSHIFT)
#define	dbtob(db)			/* calculates (db * DEV_BSIZE) */ \
	((unsigned)(db) << DEV_BSHIFT)

/*
 * Size of block device i/o is parameterized here.
 * Currently the system supports page-sized i/o.
 */
#define	BLKDEV_IOSHIFT		BPCSHIFT
#define	BLKDEV_IOSIZE		(1<<BLKDEV_IOSHIFT)

/* convert a byte offset into an offset into a logical block for a block dev */
#define	BLKDEV_OFF(off)		((off) & (BLKDEV_IOSIZE - 1))

/* convert a byte offset into a block device logical block # */
#define	BLKDEV_LBN(off)		((off) >> BLKDEV_IOSHIFT)

/* number of bb's per block device block */
#define	BLKDEV_BB		BTOBB(BLKDEV_IOSIZE)

/* convert logical block dev block to physical blocks (== bb's) */
#define	BLKDEV_LTOP(bn)		((bn) * BLKDEV_BB)

#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */

/*
 * Maximum size of hostname and domainname recognized and stored in the
 * kernel by sethostname and setdomainname
 */
#define MAXHOSTNAMELEN 256		/* can't be longer than SYS_NMLN - 1 */

/*
 * Macros for fast min/max.
 */
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */

/*
 * Macros for counting and rounding.
 */
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif
#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */

#if defined(_KERNEL) || defined(_STANDALONE)
/*
 * DELAY(n) should be n microseconds, roughly. 
 */
#define DELAY(n)	us_delay(n)
/* to guarentee delay between writes, use DELAYBUS */
#define DELAYBUS(n)	us_delaybus(n)

/* timeout call with this tick results in immediate timepoke() 
 */
#define TIMEPOKE_NOW	-100L
extern int ngroups_max;		/* master.d/kernel */
#endif /* _KERNEL || _STANDALONE */

#endif	/* _SYS_PARAM_H */
