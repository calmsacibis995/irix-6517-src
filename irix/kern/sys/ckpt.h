#ifndef __SYS_CKPT_H__
#define __SYS_CKPT_H__
/*
 * ckpt.h
 *
 *	Checkpoint/restart header file.  Contains defs internal to kernel.	
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

#ident "$Revision: 1.30 $"

typedef	void *ckpt_handle_t;

#ifdef CKPT

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#ifdef _KERNEL
/*
 * define and types for ckpt_get_ckpt interface
 */
#define	CKPT_SHMCNT	0
#define	CKPT_SHMLIST	1

typedef struct ckpt_shmlist {
	int	count;
	int	*list;
} ckpt_shmlist_t;

struct vnode;
struct cred;
struct proc;
struct prnode;
struct fid;
struct itimerval;
struct prnode;
struct uthread_s;
struct pioc_ctx;

/*
 * external defs
 */
extern int ckpt_prioctl(int, struct prnode *, caddr_t, struct cred *, int, int,
			int *);
extern int ckpt_prioctl_attr(int);
extern int ckpt_prioctl_thread(struct pioc_ctx *, struct uthread_s *,
				struct prnode *, caddr_t , struct cred *,
				int *, int *);
extern int ckpt_vp_open(struct vnode *, int, int, int *);
extern ckpt_handle_t ckpt_lookup(struct vnode *);
extern void ckpt_lookup_free(ckpt_handle_t);
extern int ckpt_lookup_add(struct vnode *, ckpt_handle_t);
extern struct fid *ckpt_lookup_get(struct vnode *, int);
extern void ckpt_vnode_free(struct vnode *vp);
extern int ckptitimer(int, struct itimerval *, struct uthread_s *);
extern int dosetitimer(int, struct itimerval *, struct itimerval *,
			struct uthread_s *);
extern void ckpt_setfd(int, int);
extern int usync_object_ckpt(struct uthread_s *, caddr_t, int *, caddr_t);
extern void ckpt_wstat_decode(int, int *, int *);

extern void ckpt_fork(struct proc *, struct proc *);
extern void ckpt_exit(struct proc *);

extern int ckpt_enabled;

#endif /* _KERNEL */

#endif /* C || C++ */

#endif /* !__CKPT_H__ */

#endif /* CKPT */
