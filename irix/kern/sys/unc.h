/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	__SYS_UNC_H
#define	__SYS_UNC_H
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

#define		UC_FORK_NOTIFY(cpid)	if (unc_enabled) uc_fork_notify(cpid)
#define		UC_EXIT_NOTIFY(why, what, pid)	if (unc_enabled) \
						uc_exit_notify(why, what, pid)

extern int	unc_enabled;

extern void	uc_fork_notify(pid_t);
extern void	uc_exit_notify(int, int, pid_t);
extern int	caienfk(void *,rval_t *);

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif
#endif	/* !__SYS_UNC_H */
