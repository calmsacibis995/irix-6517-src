/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef __SYS_LOCK_H__
#define __SYS_LOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 3.21 $"
/*
 * flags for locking procs and texts and pages
 */
#define UNLOCK      0x0000
#define PROCLOCK    0x0001
#define TXTLOCK     0x0002
#define DATLOCK     0x0004
#define PGLOCK      0x0008

#define PGLOCKALL   0x0010
#define UNLOCKALL   0x0020
#define FUTURELOCK  0x0040

#ifndef _KERNEL
extern int 	plock (int);
#else
struct pregion;
struct pas_s;
struct ppas_s;
extern int lockpages(struct pas_s *, struct ppas_s *, caddr_t, pgcnt_t);
extern int lockpages2(struct pas_s *, struct ppas_s *, caddr_t, pgcnt_t, int lpflag);
extern int unlockpages(struct pas_s *, struct ppas_s *, caddr_t, pgcnt_t, int);
extern void unlockpreg(struct pas_s *, struct ppas_s *, struct pregion *, caddr_t, pgcnt_t, int);
extern int kern_mpin(caddr_t, size_t);
extern int kern_munpin(caddr_t, size_t);

/*
 * Flag values for lpflag on lockpages2
 */
#define LPF_NONE	0			/* null flags */
#define LPF_VFAULT	1			/* doing vfault request */

#endif

#ifdef __cplusplus
}
#endif

#endif /* !__SYS_LOCK_H__ */

