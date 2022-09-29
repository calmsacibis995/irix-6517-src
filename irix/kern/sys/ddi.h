/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _IO_DDI_H	/* wrapper symbol for kernel use */
#define _IO_DDI_H	/* subject to change without notice */

#ifdef __cplusplus
extern "C" {
#endif

/*#ident	"@(#)uts-3b2:io/ddi.h	1.4"*/
#ident	"$Revision: 1.22 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/uio.h>

/*
 * ddi.h -- the flag and function definitions needed by DDI-conforming
 * drivers.  This header file contains #undefs to undefine macros that
 * drivers would otherwise pick up in order that function definitions
 * may be used. Programmers should place the include of "sys/ddi.h"
 * after any header files that define the macros #undef'ed or the code
 * may compile incorrectly.
 */

/*
 * The following macros designate a kernel parameter for drv_getparm
 * and drv_setparm. Implementation-specific parameter defines should
 * start at 100.
 */
#define	TIME	1
#define	PPGRP	3
#define	LBOLT	4
#define	SYSRINT	5
#define	SYSXINT	6
#define	SYSMINT	7
#define	SYSRAWC	8
#define	SYSCANC	9
#define	SYSOUTC	10
#define	PPID	11
#define	PSID	12
#define	UCRED	13


#ifndef NMAJORENTRY
#define NMAJORENTRY	512
#endif

extern pl_t plbase;
extern pl_t pltimeout;
extern pl_t pldisk;
extern pl_t plstr;
extern pl_t plhi;

/* The following declarations take the place of macros in 
 * sysmacros.h The undefs are for any case where a driver includes 
 * sysmacros.h, even though DDI conforming drivers must not.
 */

#undef getemajor
#undef geteminor
#undef getmajor
#undef getminor
#undef makedevice

/* STREAMS drivers and modules must include stream.h to pick up the */
/* needed structure and flag definitions. As was the case with map.h, */
/* macros used by both the kernel and drivers in times past now have */
/* a macro definition for the kernel and a function definition for */
/* drivers. The following #undefs allow drivers to include stream.h */
/* but call the functions rather than macros. */

#undef OTHERQ
#undef RD
#undef WR
#undef datamsg
#undef putnext
#undef splstr

struct pollhead;

extern int drv_getparm(ulong_t, ulong_t *);
extern clock_t drv_hztousec(clock_t);
extern int drv_setparm(ulong_t, ulong_t);
extern clock_t drv_usectohz(clock_t);
extern void drv_usecwait(clock_t);
extern major_t etoimajor(major_t);
extern major_t getemajor(dev_t);
extern minor_t geteminor(dev_t);
extern major_t getmajor(dev_t);
extern minor_t getminor(dev_t);
extern int itoemajor(major_t, major_t);
extern dev_t makedevice(major_t, minor_t);
extern int max(int, int);
extern int min(int, int);
extern ulong_t ptob(ulong_t);
extern ulong_t btop(ulong_t);
extern ulong_t btopr(ulong_t);
extern void *proc_ref(void);
extern void proc_unref(void *);
extern int proc_signal(void *, int);
extern struct pollhead *phalloc(int);
extern void phfree(struct pollhead *);
extern int physiock(int(*)(struct buf *), struct buf*, dev_t, uint64_t,
		    daddr_t, struct uio*);
struct cred;
extern int drv_priv(struct cred *);
extern toid_t itimeout(void (*)(), void *, long, pl_t, ...);
extern toid_t dtimeout(void (*)(), void *, long, pl_t, processorid_t);
extern struct queue *OTHERQ(struct queue *);	
extern struct queue *RD(struct queue *);
extern struct queue *WR(struct queue *);
extern int datamsg(uchar_t);
struct msgb;
extern int putnext(struct queue *, struct msgb *);
extern int sleep(void *, int);
extern void wakeup(void *);
extern toid_t timeout(void (*)(), void *, long, ...);
extern int untimeout(toid_t);
extern int untimeout_wait(toid_t);
extern void bcopy(const void *, void *, size_t);
extern void bzero(void *, size_t);
extern int copyin(void *, void *, int);
extern int copyout(void *,void *, int);
extern paddr_t kvtophys(void *);
extern int userabi(__userabi_t *);
extern void delay(long);
extern int spl0(void);
extern int spl1(void);
extern int spl2(void);
extern int spl3(void);
extern int spl4(void);
extern int spl5(void);
extern int spl6(void);
extern int spl7(void);
extern int splhi(void);
extern int splbase(void);
extern int spltimeout(void);
extern int spldisk(void);
extern int splstr(void);
extern int splhi(void);
extern int spltty(void);
extern void splx(int);

#ifdef __cplusplus
}
#endif

#endif	/* _IO_DDI_H */
