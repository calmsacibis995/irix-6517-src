/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _FS_PROCFS_PRSYSTM_H
#define _FS_PROCFS_PRSYSTM_H

#ident  "$Revision: 1.23 $"

#include <sys/syscall.h>
#include <sys/fault.h>
#include <sys/kmem.h>
#include <sys/proc.h>

struct pr_tracemasks {
	sysset_t	pr_entrymask;	/* syscall stop-on-entry mask */
	sysset_t	pr_exitmask;	/* syscall stop-on-exit mask */
	fltset_t	pr_fltmask;	/* fault tracing mask */
	__int64_t	pr_rval1;	/* syscall return val */
	__int64_t	pr_rval2;	/* syscall return val 2 */
	int		pr_error;	/* syscall error */
	int		pr_systrap;	/* Are any syscall mask bits set? */
};

struct proc;
struct prnode;
struct pregion;
struct uthread_s;
struct vnode;
struct cfs_handle;
struct proc_proxy_s;

extern sv_t jswait;
#define jstop_wait(px, s)	sv_bitlock_wait(&jswait, 0, \
				(uint *)(&(px)->prxy_flags), \
				PRXY_LOCK, s)
#define jstop_wake(px)		sv_broadcast(&jswait);

extern int  jsacct(proc_proxy_t *, int, int *);
extern void prawake(void);
extern void prasyncwake(void);
extern void prexit(struct proc *);
extern void prfork(struct uthread_s *, struct uthread_s *);
extern void prpollwakeup(struct prnode *, short);
extern void prfault(struct uthread_s *, uint, int *);
extern void prinvalidate(struct proc *);
extern struct vnode *prbuild(struct proc *, int);
extern void prmulti(struct proc *);
extern int  prblocktrace(struct uthread_s *);
#ifdef PRCLEAR
extern void prclear(struct proc *);
#endif

extern void prsright_release(struct proc_proxy_s *);
extern int prsright_obtain_self(void);
extern int prsright_needed(void);

extern void procfs_export_fs(struct cfs_handle *);
extern void procfs_import_fs(struct cfs_handle *);
 
#endif	/* _FS_PROCFS_PRSYSTM_H */

