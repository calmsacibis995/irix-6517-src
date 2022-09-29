/**************************************************************************
 *									  *
 *		 Copyright (C) 1995 Softway Pty Ltd			  *
 * 		 Copyright (C) 1989-1994 Silicon Graphics, Inc.		  *
 *		 All rights reserved.					  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information  of Softway Pty Ltd.,  and are  *
 *  protected by international copyright law.  They may not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Softway Pty Ltd.        *
 *									  *
 **************************************************************************/
#ifndef __SYS_HIBERNATORIISTUBS_H__
#define __SYS_HIBERNATORIISTUBS_H__
#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.5 $"

#ifdef _HIBERNATORII

extern const int HibernatorIsInstalled; /* 
                                        * 1 if Hibernator is installed;
                                        * zero otherwise 
                                        */
struct proc;
struct prnode;
struct timespec;
struct cred;
struct vnode;
/*
 * Hibernator specific initialisation
 */
extern void hib_init(void);
/*
 * Delete hibernator-specific state of old process.
 */
extern void hib_exec_remove_proc(struct proc *p);
/*
 * Delete hibernator-specific state of a process that's exited
 */
extern void hib_exit_freeproc(struct proc *p);
/*
 * Copy and modify Hibernator state from parent to child on fork.
 */
extern void hib_fork_getproc(struct proc *cp, struct proc *pp);
/*
 * Add Hibernator state to a process that joins a process group.
 * (mark open if the group is open)
 */
extern void hib_pgrp_mark_process(struct proc *);
/*
 * Delete Group-Open mark from a process that leaves a process group.
 * If the group is open, maybe wake up any waiters on the process group.
 */
extern void hib_pgrp_unmark_process(struct proc *);
/*
 * Manipulate process accounting figures to take account of accounting bias.
 */
extern void hib_acct_acctbuf(struct proc *, 
                            struct timespec *sys,
                            struct timespec *user);
/*
 * Check to see if process is groupopen before allowing
 * an O_EXCL open.
 */
extern int  hib_procfs_propen(struct proc *);
/*
 * Check to see if the caller has sufficient privilege to perform Hibernator
 * /proc ioctl.
 */
extern int  hib_procfs_check_ioctl(int cmd, struct cred *cr);
/*
 * Check to see whether Zombies are allowed for a Hibernator ioctl.
 */
extern int  hib_procfs_zdisp(int cmd);
/*
 * Perform Hibernator ioctl.
 * Returns non-zero if the ioctl is handled.
 */
extern int  hib_procfs_do_ioctl(int cmd, void *arg, struct prnode *, int *rvalp, int *error);
/*
 * Check to see if the ioctl is a write-ioctl.
 */
extern int  hib_procfs_isprwrioctl(int cmd);
/*
 * Hook for locking
 */
extern int  hib_fcntl(int cmd, void *arg, struct vnode *vp, int *rvalp, int *error);
#ifdef __cplusplus
}
#endif
#endif /* _HIBERNATORII */
#endif /* __SYS_HIBERNATORIISTUBS_H__ */
