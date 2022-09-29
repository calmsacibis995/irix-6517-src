/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_PAR_H__
#define __SYS_PAR_H__

#ident "$Revision: 1.71 $"

#ifdef __cplusplus
extern "C" {
#endif

#include "sys/syscall.h"

/*
 * /dev/par ioctl() commands:
 *
 * NOTE: /dev/par and /dev/prf share the same major device number.  Since they
 *       share absolutely no code in common this is really annoying and simply
 *       leads to potential problems when one piece of code or another is
 *       changed.  To cover this, we use ioctl() codes 0-63 (0x0-0x3f) for
 *       /dev/prf and 64-1023 (0x40-0x3ff) for /dev/par.
 */
					/* 0x0040-0x0064 unused */
#define PAR_OLDSYSCALLINH	0x0065	/* old trace syscalls for family */
					/* 0x0066 unused */
#define PAR_OLDSYSCALL		0x0067	/* old trace system calls  */
					/* 0x0068-0x006c unused */
#define PAR_SETMAXIND		0x006d	/* set maximum # of indirect bytes */
#define PAR_DISABLE		0x006e	/* disable tracing on process/family */
#define PAR_DISALLOW		0x006f	/* disallow tracing on process */
#define PAR_ALLOW		0x0070	/* allow tracing on process */
					/* 0x0071-0x007f unused */

/* the following three are always used in bit-or combination with each other */
#define PAR_SYSCALL		0x0080	/* trace system calls */
#define PAR_SWTCH		0x0100	/* trace context switches */
#define PAR_INHERIT		0x0200	/* inherit tracing */

/* reasons for idle */
#define NO_STACK	0x1	/* kernel stack is used on another processor */
#define NO_SLOAD	0x2	/* not in core */
#define MUSTRUN		0x4	/* only allowed to run on certain processor */
#define NO_FP		0x8	/* FP on another processor */
#define NO_GFX		0x10	/* graphics lock held by another process */
#define EMPTY		0x20	/* run queue empty */
#define NO_DISC		0x40	/* cpu doesn't run right sched disciplines */

/* reasons for reshedule */
#define MUSTRUNCPU	0x0001	/* must run on other cpu */
#define SEMAWAIT	0x0002	/* wait for semaphore */
#define RESCHED_P	0x0004	/* rescheduled due to user-mode preempt */
#define RESCHED_Y	0x0008	/* rescheduled due to yield */
#define RESCHED_S	0x0010	/* rescheduled due to stopped by signal */
#define RESCHED_D	0x0020	/* rescheduled due to process died */
#define MUTEXWAIT	0x0040	/* wait for mutex */
#define SVWAIT		0x0080	/* wait for synchronization variable */
#define RESCHED_KP	0x0100	/* rescheduled due to kernel preemption */
#define GANGWAIT	0x0200	/* waiting for gang members to be ready */
#define MRWAIT		0x0400	/* waiting for mrlock */

/* rescheduling due to blocking on locks and ``volunatry'' yeild */
#define RESCHED_LOCK	(SEMAWAIT|MUTEXWAIT|SVWAIT|MRWAIT)
#define RESCHED_VOL	(RESCHED_Y|RESCHED_LOCK)

/* reasons for netevents */
#define NETRES_NULL	0x00	/* null reason */
#define NETRES_MBUF	0x01	/* mbuf */
#define NETRES_QFULL	0x02	/* queue full */
#define NETRES_QEMPTY	0x03	/* queue empty */
#define NETRES_SBFULL	0x04	/* socket buffer full */
#define NETRES_SBEMPTY	0x05	/* socket buffer empty */
#define NETRES_CKSUM	0x06	/* checksum */
#define NETRES_SIZE	0x07	/* bad size packet */
#define NETRES_HEADER	0x08	/* bad packet header */
#define NETRES_FRAG	0x09	/* duplicated fagment */
#define NETRES_SYSCALL	0x0a	/* system call */
#define NETRES_INT	0x0b	/* interrupt */
#define NETRES_ERROR	0x0c	/* error */
#define NETRES_PROTCALL	0x0d	/* protocol call */
#define NETRES_PROTRET	0x0e	/* protocol return */
#define NETRES_UNREACH	0x0f	/* unreachable */
#define NETRES_CONN	0x10	/* connect call */
#define NETRES_CONNDONE	0x11	/* connect done */
#define NETRES_OFFSET	0x12	/* bad offset */
#define NETRES_NOTOURS	0x13	/* forwarding datagram */

#define	NETRES_RFSNULL	0x14
#define	NETRES_GETATTR	0x15
#define	NETRES_SETATTR	0x16
#define	NETRES_ROOT	0x17
#define	NETRES_LOOKUP	0x18
#define	NETRES_READLINK	0x19
#define	NETRES_READ	0x1a
#define	NETRES_WRTCACHE	0x1b
#define	NETRES_WRITE	0x1c
#define	NETRES_CREATE	0x1d
#define	NETRES_REMOVE	0x1e
#define	NETRES_RENAME	0x1f
#define	NETRES_LINK	0x20
#define	NETRES_SYMLINK	0x21
#define	NETRES_MKDIR	0x22
#define	NETRES_RMDIR	0x23
#define	NETRES_READDIR	0x24
#define	NETRES_STATFS	0x25
#define	NETRES_NPROC	0x26

#define NETRES_SBLOCK	0x27	/* sb lock */
#define NETRES_SBUNLOCK	0x28	/* sb unlock */
#define NETRES_SBFLUSH	0x29	/* sb flush */
#define NETRES_TIMO	0x29	/* sb flush */
#define NETRES_BUSY	0x30	/* busy */

/* netevent subtoken */
#define NETEVENT_LINKUP		0x00
#define NETEVENT_LINKDOWN	0x01
#define NETEVENT_IPUP		0x02
#define NETEVENT_IPDOWN		0x03
#define NETEVENT_UDPUP		0x04
#define NETEVENT_UDPDOWN	0x05
#define NETEVENT_TCPUP		0x06
#define NETEVENT_TCPDOWN	0x07
#define NETEVENT_SOUP		0x08
#define NETEVENT_SODOWN		0x09
#define NETEVENT_NFSUP		0x0a
#define NETEVENT_NFSDOWN	0x0b
#define NETEVENT_RAWUP		0x0c
#define NETEVENT_RAWDOWN	0x0d
#define NETEVENT_ARPUP		0x0e
#define NETEVENT_ARPDOWN	0x0f
#define NETEVENT_RPCUP		0x10
#define NETEVENT_RPCDOWN	0x11
#define NETEVENT_DDNUP		0x12
#define NETEVENT_DDNDOWN	0x13

/* misc for netevent */
#define NETCNT_NULL	((char)-99)
#define NETPID_NULL	((char)-98)

#ifdef _KERNEL
struct sysent;
extern void fawlty_exec(const char*);
extern void fawlty_fork(int64_t, int64_t, const char*);
extern void fawlty_sys(short, struct sysent *, sysarg_t *);
extern void fawlty_sysend(short, struct sysent *, sysarg_t *, __int64_t, __int64_t, int);
struct kthread;
extern void fawlty_resched(struct kthread*, int);

#ifdef NET_DEBUG
#define NETPAR(f,t,p,s,l,r)						\
    LOG_TSTAMP_EVENT(f, t, (p), RTMON_PACKNET(s,r), (l), NULL)
#else
#define NETPAR(f,t,p,s,l,r)
#endif /* NET_DEBUG */

struct buf;
extern void fawlty_disk(int,struct buf *);
#define DISKSTATE(evt,bp) {		/* NB: bp is used twice */	\
    if (IS_TSTAMP_EVENT_ACTIVE(RTMON_DISK) && bp)			\
	fawlty_disk(evt,(bp));						\
}
#endif /* _KERNEL */

/*
 * Vestiges of old par stuff that belong in rtmon.h.
 */
#define FAWLTY_MAX_PARAMS	8
/*
 * NOTE: kernel and padc code depends upon the fact that the size of the
 * callrec struct preceeding the params array may be copied by integer
 * (that is, 4,8,12, etc. bytes long).  All hell will break loose if a
 * field is added before params that makes it byte-aligned (if the compiler
 * permits it without rounding).
 *
 * Following the parameters are the indirect parameters:
 *	descriptor (short)
 *	length (short)
 *	value ..
 *
 * For variable length indirect parameters (like read/write) we don't
 * want infinite amounts of data - we start by limiting it to PAR_DEFINDBYTES
 * This can be changed up to PAR_MAXINDBYTES via an ioctl.
 * Note that this doesn't apply to strings.
 * The 4096 is somewhat arbitrary but expanding it much more will probably
 * require some changes to the buffering in prf.c
 */
#define PAR_DEFINDBYTES	32
#define PAR_MAXINDBYTES	4096

#ifdef __cplusplus
}
#endif
#endif /* __SYS_PAR_H__ */
