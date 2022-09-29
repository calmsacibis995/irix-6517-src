/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/kutils/RCS/syscall.h,v 65.3 1999/07/21 19:01:14 gwehrman Exp $ */

#ifndef	TRANSARC_KUTILS_SYSCALL_H_
#define	TRANSARC_KUTILS_SYSCALL_H_
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#include <dcedfs/stds.h>

/*
 * System call number for afscalls.
 */
#ifndef AFS_SYSCALL
#ifdef __OSF1__
#define AFS_SYSCALL     258
#elif defined(AFS_SUNOS5_ENV)
#define AFS_SYSCALL      72
#elif defined(AFS_HPUX_ENV)
#define AFS_SYSCALL	 50
#define AFS_SYSCALL_HPUX_10	326
#elif defined AFS_IRIX_ENV
#define AFS_SYSCALL	 206 
#else
#define AFS_SYSCALL      31
#endif /* OSF1 */
#endif /*AFS_SYSCALL */
/*
 * Entries to AFS kernel modules; add as needed
 */

/*
 * But first--a word from HP, who have gratuitously put a definition of
 * AFSCALL_KLOAD in one of their own include files, so that we have to
 * blow it away first.
 */
#ifdef	AFS_HPUX_ENV
#ifdef	AFSCALL_KLOAD
#undef	AFSCALL_KLOAD
#endif
#endif

#define	AFSCALL_CM	0	/* Main syscall entry to the Cache Manager */
#define	AFSCALL_PIOCTL	1	/* XXX pioctl(2) cm call XXX */
#define	AFSCALL_SETPAG	2	/* setpag(2) syscall */
#define	AFSCALL_PX	3	/* syscall entry to the Decorum exporter */
#define	AFSCALL_VOL	4	/* syscall entry to the volume module */
#define	AFSCALL_AGGR	5	/* syscall entry to the aggregate module */
#define	AFSCALL_EPISODE	6	/* syscall entry to Episode */
#define AFSCALL_KTC	7	/* XXX Kerberos Ticket Cache XXX */
#define	AFSCALL_FP	8	/* XXX Temp: Test the fp Module XXX */
#define	AFSCALL_XCRED	9	/* XXX Temp: Test the xcred Module XXX */
#define AFSCALL_VNODE_OPS 10	/* syscall entry for extended vnode ops */
#define AFSCALL_GETPAG	11	/* getpag call */
#define AFSCALL_PLUMBER	12	/* plumber call */
#define AFSCALL_TKM	13	/* token manager control */
#define AFSCALL_ICL	14	/* In-core log control */
#define AFSCALL_KRPCDBG 15
#define AFSCALL_NEWTGT  16	/* newtgt (2) cm system call */
#define AFSCALL_BOMB	17	/* bomb point interface */
#define AFSCALL_KLOAD	18	/* HP/UX kernel extension load */
#define AFSCALL_AT      19      /* gateway authentication table */
#define	AFSCALL_RESETPAG 20	/* reset PAG syscall */
#define	AFSCALL_LAST	20	/* Adjust as new entries are added */

/*
 * XXX Although seemingly irrelevant to this include file, the following struct
 * is used in most i/o related system calls; the initial motivator was to cut
 * down the max arguments to below 6 but it had additional benefits after
 * all... XXX
 */
struct io_rwDesc {
    int descId;		/* File/object identifier */
    int position;
    int length;
    char *bufferp;
};

struct io_rwHyperDesc {
    int descId;		/* File/object identifier */
    afs_hyper_t position;
    int length;
    char *bufferp;
};

#ifndef _KERNEL

#ifdef SGIMIPS
extern afs_syscall(long, long, long, long, long, long);
#else
extern afs_syscall(int, int, int, int, int, int);
#endif /* SGIMIPS */

#endif /* !_KERNEL */

#ifdef _KERNEL

#define AfsCall(index, proc) afs_sysent[(index)].afs_call = (proc)
#define AfsProbe(index) (afs_sysent[(index)].afs_call)

#ifdef SGIMIPS
struct afs_sysent {
    int (*afs_call)(long, long, long, long, long, long *);
};
#else
struct afs_sysent {
    int (*afs_call)(long, long, long, long, long, int *);
};
#endif

extern struct afs_sysent afs_sysent[];

#ifdef SGIMIPS

extern void afs_set_syscall(
    int entry,
    int (*call)(long, long, long, long, long, long *)
);
#else
extern void afs_set_syscall(
    int entry,
    int (*call)(long, long, long, long, long, int *)
);
#endif

#ifndef AFS_DYNAMIC

typedef int (*CFG_RTN)();

extern CFG_RTN config_rtn[];
extern short config_done[];

#endif /* !AFS_DYNAMIC */

extern u_int afscall_timeSynchDistance;
extern u_int afscall_timeSynchDispersion;

#endif /* _KERNEL */

#endif	/* TRANSARC_KUTILS_SYSCALL_H_ */
